/*
 * FogLAMP "rate" filter plugin.
 *
 * Copyright (c) 2018 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */

#include <plugin_api.h>
#include <config_category.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string>
#include <iostream>
#include <filter_plugin.h>
#include <rate_filter.h>
#include <version.h>

#define FILTER_NAME "rate"
#define DEFAULT_CONFIG "{\"plugin\" : { \"description\" : \"Variable readings collection rate filter\", " \
                       		"\"type\" : \"string\", " \
				"\"default\" : \"" FILTER_NAME "\",\"readonly\" : \"true\" }, " \
			 "\"enable\": {\"description\": \"A switch that can be used to enable or disable execution of " \
					 "the rate filter.\", " \
				"\"type\": \"boolean\", " \
				"\"displayName\": \"Enabled\", " \
				"\"default\": \"false\" }, " \
			 "\"trigger\": {\"description\": \"Expression to trigger full rate collection\", " \
				"\"type\": \"string\", " \
				"\"default\": \"\", \"order\" : \"1\", \"displayName\" : \"Trigger expression\" }, " \
			 "\"untrigger\": {\"description\": \"Expression to trigger end of full rate collection\", " \
				"\"type\": \"string\", " \
				"\"default\": \"\", \"order\" : \"2\", \"displayName\" : \"End Expression\" }, " \
			 "\"preTrigger\": {\"description\": \"The amount of data to send prior to the trigger firing, expressed in milliseconds\", " \
				"\"type\": \"integer\", " \
				"\"default\": \"1\", \"order\" : \"3\", " \
				"\"displayName\" : \"Pre-trigger time (mS)\" }, " \
			 "\"rate\": {\"description\": \"The reduced rate at which data must be sent\", " \
				"\"type\": \"integer\", " \
				"\"default\": \"0\", \"order\" : \"4\", " \
				"\"displayName\" : \"Reduced collection rate\" }, " \
			 "\"rateUnit\": {\"description\": \"The unit used to evaluate the reduced rate\", " \
				"\"type\": \"enumeration\", " \
				"\"options\" : [ \"per second\", \"per minute\", \"per hour\", \"per day\" ], " \
				"\"default\": \"per second\", \"order\" : \"5\", " \
				"\"displayName\" : \"Rate Units\" } " \
			"}"

using namespace std;

/**
 * The Filter plugin interface
 */
extern "C" {

/**
 * The plugin information structure
 */
static PLUGIN_INFORMATION info = {
        FILTER_NAME,              // Name
        VERSION,                  // Version
        0,                        // Flags
        PLUGIN_TYPE_FILTER,       // Type
        "1.0.0",                  // Interface version
	DEFAULT_CONFIG	          // Default plugin configuration
};

typedef struct
{
	RateFilter	*handle;
	std::string	configCatName;
} FILTER_INFO;

/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info()
{
	return &info;
}

/**
 * Initialise the rate plugin. This creates the underlying
 * object and prepares the filter for operation. This will be
 * called before any data is ingested
 *
 * @param config	The configuration category for the filter
 * @param outHandle	A handle that will be passed to the output stream
 * @param output	The output stream (function pointer) to which data is passed
 * @return		An opaque handle that is used in all subsequent calls to the plugin
 */
PLUGIN_HANDLE plugin_init(ConfigCategory *config,
			  OUTPUT_HANDLE *outHandle,
			  OUTPUT_STREAM output)
{
	FILTER_INFO *info = new FILTER_INFO;
	info->handle = new RateFilter(FILTER_NAME,
					*config,
					outHandle,
					output);
	info->configCatName = config->getName();
	
	return (PLUGIN_HANDLE)info;
}

/**
 * Ingest a set of readings into the plugin for processing
 *
 * @param handle	The plugin handle returned from plugin_init
 * @param readingSet	The readings to process
 */
void plugin_ingest(PLUGIN_HANDLE *handle,
		   READINGSET *readingSet)
{
	FILTER_INFO *info = (FILTER_INFO *) handle;
	RateFilter *filter = info->handle;
	if (!filter->isEnabled())
	{
		/*
		 * Current filter is not active: just pass the readings
		 * set along the filter chain
		 */
		filter->m_func(filter->m_data, readingSet);
		return;
	}

	/*
	 * Create a new vector for the output readings. This may contain
	 * a mixture of readings created by the plugin and readings from
	 * the reading set passed in. The filter class takes care of
	 * deleting any readings not passed up the chain.
	 *
	 * We create a new ReadingSet with the contains of the output
	 * vector, hence the readingSet passed in is deleted.
	 */
	vector<Reading *> out;
	filter->ingest(((ReadingSet *)readingSet)->getAllReadingsPtr(), out);
	const vector<Reading *>& readings = ((ReadingSet *)readingSet)->getAllReadings();
	for (vector<Reading *>::const_iterator elem = readings.begin();
						      elem != readings.end();
						      ++elem)
	{
		AssetTracker::getAssetTracker()->addAssetTrackingTuple(info->configCatName, (*elem)->getAssetName(), string("Filter"));
	}
	delete (ReadingSet *)readingSet;

	/*
	 * Create a new reading set and pass it up the filter
	 * chain. Note this reading set may not contain any
	 * actual readings.
	 */
	ReadingSet *newReadingSet = new ReadingSet(&out);
	const vector<Reading *>& readings2 = newReadingSet->getAllReadings();
	for (vector<Reading *>::const_iterator elem = readings2.begin();
						      elem != readings2.end();
						      ++elem)
	{
		AssetTracker::getAssetTracker()->addAssetTrackingTuple(info->configCatName, (*elem)->getAssetName(), string("Filter"));
	}
	filter->m_func(filter->m_data, newReadingSet);
}

/**
 * Reconfigure the plugin
 *
 * @param handle	The plugin handle
 * @param newConfig	The updated configuration
 */
void plugin_reconfigure(PLUGIN_HANDLE *handle, const string& newConfig)
{
	FILTER_INFO *info = (FILTER_INFO *) handle;
	RateFilter *filter = info->handle;
	filter->reconfigure(newConfig);
}

/**
 * Call the shutdown method in the plugin
 */
void plugin_shutdown(PLUGIN_HANDLE *handle)
{
	FILTER_INFO *info = (FILTER_INFO *) handle;
	delete info->handle;
	delete info;
}

};
