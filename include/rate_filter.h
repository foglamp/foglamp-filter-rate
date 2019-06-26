#ifndef _RATE_FILTER_H
#define _RATE_FILTER_H
/*
 * FogLAMP "rate" filter plugin.
 *
 * Copyright (c) 2018 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch           
 */     
#include <filter.h>               
#include <reading_set.h>
#include <config_category.h>
#include <string>                 
#include <logger.h>
#include <list>
#include <vector>
#include <exprtk.hpp>
#include <mutex>

#define MAX_EXPRESSION_VARIABLES 20

/**
 * A FogLAMP filter that allows variable rates of data to be sent.
 * It uses trigger expressions to triggr the sending of full rate
 * readings. When the filter is not triggered it averages readings
 * at a rate defined in the cnfiguration and sends averages for
 * those time periods.
 *
 * Triggering and triggering return to the averaging behavior is
 * performed by use of expression in the configuration. These
 * expressions use the data points in the reading as variables
 * within the expression.
 *
 * TODO Currently the filter is limited to stream with a single
 * asset per stream. It should be enhanced to support multiple
 * assets.
 */
class RateFilter : public FogLampFilter {
	public:
		RateFilter(const std::string& filterName,
                        ConfigCategory& filterConfig,
                        OUTPUT_HANDLE *outHandle,
                        OUTPUT_STREAM out);
		~RateFilter();
		void	setTrigger(const std::string& expression)
			{
				m_trigger = expression;
			};
		void	setUntrigger(const std::string& expression)
			{
				m_untrigger = expression;
			};
		void	setPreTrigger(int pretrigger)
			{
				m_pretrigger = pretrigger;
			}
		void	ingest(std::vector<Reading *> *readings, std::vector<Reading *>& out);
		void	reconfigure(const std::string& newConfig);
	private:
		void	triggeredIngest(std::vector<Reading *> *readings, std::vector<Reading *>& out);
		void	untriggeredIngest(std::vector<Reading *> *readings, std::vector<Reading *>& out);
		void	sendPretrigger(std::vector<Reading *>& out);
		void	sendPretrigger(std::vector<Reading *>& out, Reading *trigger);
		void	bufferPretrigger(Reading *);
		void	addAverageReading(Reading *, std::vector<Reading *>& out);
		void	addDataPoint(const std::string&, double);
		Reading *averageReading(Reading *);
		void	clearAverage();
		void 	handleConfig(const ConfigCategory& conf);
		bool	isExcluded(const std::string& asset);
		class Evaluator {
			public:
				Evaluator(Reading *, const std::string& expression);
				bool		evaluate(Reading *);
			private:
				exprtk::expression<double>	m_expression;
				exprtk::symbol_table<double>	m_symbolTable;
				exprtk::parser<double>		m_parser;
				double				m_variables[MAX_EXPRESSION_VARIABLES];
				std::string			m_variableNames[MAX_EXPRESSION_VARIABLES];
				int				m_varCount;
		};
		std::string		m_trigger;
		std::string		m_untrigger;
		struct timeval		m_rate;
		struct timeval		m_lastSent;
		int			m_pretrigger;
		std::list<Reading *>	m_buffer;
		bool			m_state;
		bool			m_pendingReconfigure;
		std::mutex		m_configMutex;
		Evaluator		*m_triggerExpression;
		Evaluator		*m_untriggerExpression;
		int			m_averageCount;
		std::map<std::string, double>
					m_averageMap;
		std::vector<std::string>
					m_exclusions;
		std::string		m_pretriggerFilter;
};


#endif
