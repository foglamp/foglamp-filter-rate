/*
 * FogLAMP "rate" filter plugin.
 *
 * Copyright (c) 2018 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch           
 */     
#include <reading.h>              
#include <reading_set.h>
#include <utility>                
#include <logger.h>
#include <exprtk.hpp>
#include <rate_filter.h>

using namespace std;
using namespace rapidjson;

/**
 * Construct a RateFilter, call the base class constructor and handle the
 * parsing of the configuration category the required rate
 */
RateFilter::RateFilter(const std::string& filterName,
			       ConfigCategory& filterConfig,
                               OUTPUT_HANDLE *outHandle,
                               OUTPUT_STREAM out) :
                                  FogLampFilter(filterName, filterConfig,
                                                outHandle, out),
				  m_state(false), m_pretrigger(0), m_averageCount(0),
				  m_triggerExpression(0), m_untriggerExpression(0)
{
	m_lastSent.tv_sec = 0;
	m_lastSent.tv_usec = 0;
	handleConfig(filterConfig);
}

/**
 * Destructor for the filter.
 */
RateFilter::~RateFilter()
{
}

/**
 * Called with a set of readings, iterates over the readings applying
 * the rate filter to create the output readings
 *
 * @param readings	The readings to process
 * @param out		The output readings
 */
void RateFilter::ingest(vector<Reading *> *readings, vector<Reading *>& out)
{
	lock_guard<mutex> guard(m_configMutex);
	if (m_pendingReconfigure)
	{
		if (m_triggerExpression)
			delete m_triggerExpression;
		if (m_untriggerExpression)
			delete m_untriggerExpression;
		m_triggerExpression = 0;
		m_untriggerExpression = 0;
		m_pendingReconfigure = false;
	}
	// Use the first reading to create the evaluators if we do not already have them
	if (m_triggerExpression == 0)
	{
		Reading *firstReading = readings->front();
		m_triggerExpression = new Evaluator(firstReading, m_trigger);
		if (!m_untrigger.empty())
		{
			m_untriggerExpression = new Evaluator(firstReading, m_untrigger);
		}
		else
		{
			m_untriggerExpression = new Evaluator(firstReading, string("! (")
							+ m_untrigger + string(")"));
		}
	}
	if (m_state)
	{
		triggeredIngest(readings, out);
	}
	else
	{
		untriggeredIngest(readings, out);
	}
}

/**
 * Called when in the triggered state to forward the readings and evaluate the
 * untrigger expression. If the state changes to untrigger then the untriggerIngest
 * method will be called.
 *
 * @param readings	The readings to process
 * @param out		The output readings
 */
void RateFilter::triggeredIngest(vector<Reading *> *readings, vector<Reading *>& out)
{
int	offset = 0;	// Offset within the vector

	for (vector<Reading *>::const_iterator reading = readings->begin();
						      reading != readings->end();
						      ++reading)
	{
		if (m_untriggerExpression->evaluate(*reading))
		{
			m_state = false;
			// Remove the readings we have dealt with
			readings->erase(readings->begin(), readings->begin() + offset);
			return untriggeredIngest(readings, out);
		}
		out.push_back(*reading);
		offset++;
	}
	readings->clear();
}

/**
 * Called when in the triggered state to average the readings and evaluate the
 * trigger expression. If the state changes to untrigger then the triggerIngest
 * method will be called.
 *
 * @param readings	The readings to process
 * @param out		The output readings
 */
void RateFilter::untriggeredIngest(vector<Reading *> *readings, vector<Reading *>& out)
{
int	offset = 0;	// Offset within the vector

	for (vector<Reading *>::const_iterator reading = readings->begin();
						      reading != readings->end();
						      ++reading)
	{
		if (isExcluded((*reading)->getAssetName()))
		{
			out.push_back(*reading);
		}
		else
		{
			if (m_triggerExpression->evaluate(*reading))
			{
				m_state = true;
				clearAverage();
				// Remove the readings we have dealt with
				readings->erase(readings->begin(), readings->begin() + offset);
				sendPretrigger(out, *reading);
				return triggeredIngest(readings, out);
			}
			bufferPretrigger(*reading);
			if (m_rate.tv_sec != 0 || m_rate.tv_usec != 0)
			{
				addAverageReading(*reading, out);
			}
			delete *reading;
		}
		offset++;
	}
	readings->clear();
}

/**
 * If we have a pretrigger buffer defined in the configuration then
 * keep a copy of the reading int he pretrigger buffer. Remove any readings
 * that are older than the defined pretrigger age.
 *
 * @param reading	Reading to buffer
 */
void RateFilter::bufferPretrigger(Reading *reading)
{
Reading	*t;
struct timeval	now, t1, t2, res;

	if (m_pretrigger == 0)	// No pretrigger buffering
	{
		return;
	}
	m_buffer.push_back(new Reading(*reading));

	/*
	 * Remove the entries from the front of the pretrigger buffer taht are
	 * older than the pre trigger time.
	 */
	t2.tv_sec = m_pretrigger / 1000;
	t2.tv_usec = (m_pretrigger % 1000) * 1000;
	for (;;)
	{
		t = m_buffer.front();
		t->getUserTimestamp(&t1);
		reading->getUserTimestamp(&now);
		timersub(&now, &t1, &res);
		if (timercmp(&res, &t2, >))
		{
			t = m_buffer.front();
			delete t;
			m_buffer.pop_front();
		}
		else
		{
			break;
		}
	}
}

/**
 * Send the pretigger buffer data
 *
 * @param out	The output buffer
 */
void RateFilter::sendPretrigger(vector<Reading *>& out)
{
	while (!m_buffer.empty())
	{
		Reading *r = m_buffer.front();
		out.push_back(r);
		m_buffer.pop_front();
	}
}

/**
 * Send the pretigger buffer data, filtering the given data point in the reading that
 * triggered the sending of the buffer.
 *
 * @param out	The output buffer
 * @param reading	The readign that caused the pretrigger to be send
 */
void RateFilter::sendPretrigger(vector<Reading *>& out, Reading *reading)
{
DatapointValue	*match = NULL;

	if (m_pretriggerFilter.compare(""))
	{
		vector<Datapoint *> dps = reading->getReadingData();
		for (auto it = dps.cbegin(); it != dps.end(); ++it)
		{
			if ((*it)->getName().compare(m_pretriggerFilter) == 0)
			{
				match = &((*it)->getData());
			}
		}
	}
	while (!m_buffer.empty())
	{
		Reading *r = m_buffer.front();
		if (match == NULL)
		{
			out.push_back(r);
		}
		else
		{
			bool addReading = false;
			vector<Datapoint *> dps = r->getReadingData();
			for (auto it = dps.cbegin(); addReading == false && it != dps.end(); ++it)
			{
				if ((*it)->getName().compare(m_pretriggerFilter) == 0)
				{
					DatapointValue dp  = ((*it)->getData());
					if (dp.getType() == match->getType())
					{
						switch (dp.getType())
						{
							case DatapointValue::T_INTEGER:
								addReading = (dp.toInt() == match->toInt());
								break;
							case DatapointValue::T_FLOAT:
								addReading = (dp.toDouble() == match->toDouble());
								break;
						}
					}
				}
			}
			if (addReading)
			{
				out.push_back(r);
			}
			else
			{
				delete r;
			}
		}
		m_buffer.pop_front();
	}
}

/**
 * Add a reading to the average data. If the period has enxpired in which
 * to send a reading then the average will be calculated and added to the
 * out buffer.
 *
 * @param reading	The reading to add
 * @param out		The output buffer to add any average to.
 */
void RateFilter::addAverageReading(Reading *reading, vector<Reading *>& out)
{
	vector<Datapoint *>	datapoints = reading->getReadingData();
	for (auto it = datapoints.begin(); it != datapoints.end(); it++)
	{
		DatapointValue& dpvalue = (*it)->getData();
		if (dpvalue.getType() == DatapointValue::T_INTEGER)
		{
			addDataPoint((*it)->getName(), (double)dpvalue.toInt());
		}
		if (dpvalue.getType() == DatapointValue::T_FLOAT)
		{
			addDataPoint((*it)->getName(), dpvalue.toDouble());
		}
	}
	m_averageCount++;
	
	struct timeval t1, res;
	reading->getUserTimestamp(&t1);
	timeradd(&m_lastSent, &m_rate, &res);
	if (timercmp(&t1, &res, >))
	{
		out.push_back(averageReading(reading));
		m_lastSent = t1;
	}
}

/**
 * Add a data point to the average data
 *
 * @param name	The datapoint name
 * @param value	The datapoint value
 */
void RateFilter::addDataPoint(const string& name, double value)
{
	map<string, double>::iterator it = m_averageMap.find(name);
	if (it != m_averageMap.end())
	{
		it->second += value;
	}
	else
	{
		m_averageMap.insert(pair<string, double>(name, value));
	}
}

/**
 * Create a average reading using the asset name and times from the reading
 * passed in and the data accumulated in the average map
 *
 * @param reading	The reading to take the asset name and times from
 */
Reading *RateFilter::averageReading(Reading *templateReading)
{
string			asset = templateReading->getAssetName();
vector<Datapoint *>	datapoints;

	for (map<string, double>::iterator it = m_averageMap.begin();
				it != m_averageMap.end(); it++)
	{
		DatapointValue dpv(it->second / m_averageCount);
		it->second = 0.0;
		datapoints.push_back(new Datapoint(it->first, dpv));
	}
	m_averageCount = 0;
	Reading	*rval = new Reading(asset, datapoints);
	struct timeval tm;
	templateReading->getUserTimestamp(&tm);
	rval->setUserTimestamp(tm);
	templateReading->getTimestamp(&tm);
	rval->setTimestamp(tm);
	return rval;
}

/**
 * Clear the average data having triggered a change of state
 *
 */
void RateFilter::clearAverage()
{
	for (map<string, double>::iterator it = m_averageMap.begin();
				it != m_averageMap.end(); it++)
	{
		it->second = 0.0;
	}
}

/**
 * Constructor for the evaluator class. This holds the expressions and
 * variable bindings used to execute the triggers.
 *
 * @param reading	An initial reading to use to create varaibles
 * @parsm expression	The expression to evaluate
 */
RateFilter::Evaluator::Evaluator(Reading *reading, const string& expression) : m_varCount(0)
{
	vector<Datapoint *>	datapoints = reading->getReadingData();
	for (auto it = datapoints.begin(); it != datapoints.end(); it++)
	{
		DatapointValue& dpvalue = (*it)->getData();
		if (dpvalue.getType() == DatapointValue::T_INTEGER ||
				dpvalue.getType() == DatapointValue::T_FLOAT)
		{
			m_variableNames[m_varCount++] = (*it)->getName();
			m_variableNames[m_varCount++] = reading->getAssetName() + "." + (*it)->getName();
		}
		if (m_varCount == MAX_EXPRESSION_VARIABLES)
		{
			Logger::getLogger()->error("Too many datapoints in reading");
			break;
		}
	}

	for (int i = 0; i < m_varCount; i++)
	{
		m_symbolTable.add_variable(m_variableNames[i], m_variables[i]);
	}
	m_symbolTable.add_constants();
	m_expression.register_symbol_table(m_symbolTable);
	m_parser.compile(expression.c_str(), m_expression);
}

/**
 * Evaluate an expression using the reading provided and return true of false. 
 *
 * @param	reading	The reading from which the variables are taken
 * @return	Bool result of evaluatign the expression
 */
bool RateFilter::Evaluator::evaluate(Reading *reading)
{
	vector<Datapoint *> datapoints = reading->getReadingData();
	for (auto it = datapoints.begin(); it != datapoints.end(); it++)
	{
		string name = (*it)->getName();
		double value = 0.0;
		DatapointValue& dpvalue = (*it)->getData();
		if (dpvalue.getType() == DatapointValue::T_INTEGER)
		{
			value = dpvalue.toInt();
		}
		else if (dpvalue.getType() == DatapointValue::T_FLOAT)
		{
			value = dpvalue.toDouble();
		}
		
		string fullname = reading->getAssetName() + "." + name;
		for (int i = 0; i < m_varCount; i++)
		{
			if (m_variableNames[i].compare(name) == 0)
			{
				m_variables[i] = value;
			}
			if (m_variableNames[i].compare(fullname) == 0)
			{
				m_variables[i] = value;
			}
		}
	}
	return m_expression.value() != 0.0;
}

/**
 * Handle a reconfiguration request
 *
 * @param newConfig	The new configuration
 */
void RateFilter::reconfigure(const string& newConfig)
{
	lock_guard<mutex> guard(m_configMutex);
	setConfig(newConfig);
	handleConfig(m_config);
	m_pendingReconfigure = true;
}


/**
 * Handle the configuration of the plugin.
 *
 *
 * @param conf	The configuration category for the filter.
 */
void RateFilter::handleConfig(const ConfigCategory& config)
{
	setTrigger(config.getValue("trigger"));
	setUntrigger(config.getValue("untrigger"));
	m_pretrigger = strtol(config.getValue("preTrigger").c_str(), NULL, 10);

	int rate = strtol(config.getValue("rate").c_str(), NULL, 10);
	string unit = config.getValue("rateUnit");
	if (rate == 0)
	{
		m_rate.tv_sec = 0;
		m_rate.tv_usec = 0;
	}
	else if (unit.compare("per second") == 0)
	{
		m_rate.tv_sec = 0;
		m_rate.tv_usec = 1000000 / rate;
	}
	else if (unit.compare("per minute") == 0)
	{
		m_rate.tv_sec = 60 / rate;
		m_rate.tv_usec = 0;
	}
	else if (unit.compare("per hour") == 0)
	{
		m_rate.tv_sec = 3600 / rate;
		m_rate.tv_usec = 0;
	}
	else if (unit.compare("per day") == 0)
	{
		m_rate.tv_sec = (24 * 60 * 60) / rate;
		m_rate.tv_usec = 0;
	}
	string m_pretriggerFilter = config.getValue("pretriggerFilter");
	string exclusions = config.getValue("exclusions");
	rapidjson::Document doc;
	doc.Parse(exclusions.c_str());
	if (!doc.HasParseError())
	{
		if (doc.HasMember("exclusions") && doc["exclusions"].IsArray())
		{
			const rapidjson::Value& values = doc["exclusions"];
			for (rapidjson::Value::ConstValueIterator itr = values.Begin();
                                                itr != values.End(); ++itr)
                        {
				if (itr->IsString())
				{
					m_exclusions.push_back(itr->GetString());
				}
				else
				{
					Logger::getLogger()->error("The exclusions element should be an array of strings");
				}
			}

		}
		else
		{
			Logger::getLogger()->error("The exclusions element should be an array of strings");
		}
	}
	else
	{
		Logger::getLogger()->error("Error parsing the exlcusions element. The exclusions element should be an array of strings");
	}
}


/**
 * Check if the asset name is in the exclusions list
 *
 * @param name	The asset name to check
 * @return true if the asset is exempt from the rate limiting
 */
bool RateFilter::isExcluded(const string& asset)
{
	for (auto it = m_exclusions.cbegin(); it != m_exclusions.cend(); ++it)
	{
		if (asset.compare(*it) == 0)
		{
			return true;
		}
	}
	return false;
}
