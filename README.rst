=====================================
Fledge "rate" C++ Filter plugin
=====================================

A filter plugin that can be used to reduce the rate a reading is stored
until an interesting event occurs. The filter will read data at full
rate from the input side and buffer data internally, sending out averages
for each value over a time frame determined by the filter configuration.

The user will provide two simple expressions that will be evaluated to
form a trigger for the filter. One expressions will set the trigger and
the other will clear it. When the trigger is set then the filter will
no longer average the data over the configured time period, but will
instead send the full bandwidth data out of the filter.

The filter also allows a pre-trigger time to be configured. In this
case it will buffer this much data internally and when the trigger is
initially set this pre-buffered data will be sent. The pre-buffered data
is discarded if the trigger is not set and the data gets to the defined
age for holding pre-trigger information.

The configuration items for the filter are:

  - The nominal data rate to send data out. This defines the period over which is outgoing data item is averaged.

  - An expression to set the trigger for full rate data

  - The mechanism to stop full rate forwarding, this may be another expression or a time window

  - An expression to clear the trigger for full rate data, if left blank this will simply be the trigger filter evaluating to false

  - The time window, in milliseconds to forward data at the full rate

  - An optional pre-trigger time expressed in milliseconds

  - A set of asset names that are excluded from the rate limit processing and always sent at full rate

For example if the filter is working with a SensorTag and it reads the tag
data at 10ms intervals but we only wish to send 1 second averages under
normal circumstances. However if the X axis acceleration exceed 1.5g
then we want to send full bandwidth data until the X axis acceleration
drops to less than 0.2g, and we also want to see the data for the 1
second before the acceleration hit this peak the configuration might be:

Nominal Data Rate: 1, data rate unit "per second"

Trigger set expression: X > 1.5

Trigger clear expression: X < 0.2

Pre-trigger time (mS): 1000

The trigger expression uses the same expression mechanism as the
fledge-south-expression and fledge-filter-expression plugins

Expression may contain any of the following...

- Mathematical operators (+, -, *, /, %, ^)

- Functions (min, max, avg, sum, abs, ceil, floor, round, roundn, exp, log, log10, logn, pow, root, sqrt, clamp, inrange, swap)

- Trigonometry (sin, cos, tan, acos, asin, atan, atan2, cosh, cot, csc, sec, sinh, tanh, d2r, r2d, d2g, g2d, hyp)

- Equalities & Inequalities (=, ==, <>, !=, <, <=, >, >=)

- Logical operators (and, nand, nor, not, or, xor, xnor, mand, mor)

The plugin uses the C++ Mathematical Expression Toolkit Library
by Arash Partow and is used under the MIT licence granted on that toolkit.

Note this plugin is designed to work with streams with a single asset
in the stream, there is no mechanism in the expression syntax to support
multiple asset names.

Example Configuration
---------------------

The following is an example configuration.

.. code-block:: console

  {
    "untrigger": {
      "description": "Expression to trigger end of full rate collection",
      "type": "string",
      "order": "2",
      "value": "Vibration < 1",
      "default": ""
    },
    "rate": {
      "description": "The reduced rate at which data must be sent",
      "type": "integer",
      "order": "4",
      "value": "1",
      "default": "0"
    },
    "rateUnit": {
      "description": "The unit used to evaluate the reduced rate",
      "order": "5",
      "type": "enumeration",
      "options": [
        "per second",
        "per minute",
        "per hour",
        "per day"
      ],
      "value": "per second",
      "default": "per second"
    },
    "plugin": {
      "description": "Expression filter plugin",
      "type": "string",
      "default": "rate",
      "value": "rate",
      "readonly": "true"
    },
    "enable": {
      "description": "A switch that can be used to enable or disable execution of the rate filter.",
      "type": "boolean",
      "default": "false",
      "value": "true"
    },
    "trigger": {
      "description": "Expression to trigger full rate collection",
      "type": "string",
      "order": "1",
      "value": "Vibration > 1",
      "default": ""
    },
    "preTrigger": {
      "description": "The amount of data to send prior to the trigger firing, expressed in milliseconds",
      "type": "integer",
      "order": "3",
      "value": "0",
      "default": "1"
    }
  }

Build
-----
To build Fledge "rate" C++ filter plugin:

.. code-block:: console

  $ mkdir build
  $ cd build
  $ cmake ..
  $ make

- By default the Fledge develop package header files and libraries
  are expected to be located in /usr/include/fledge and /usr/lib/fledge
- If **FLEDGE_ROOT** env var is set and no -D options are set,
  the header files and libraries paths are pulled from the ones under the
  FLEDGE_ROOT directory.
  Please note that you must first run 'make' in the FLEDGE_ROOT directory.

You may also pass one or more of the following options to cmake to override 
this default behaviour:

- **FLEDGE_SRC** sets the path of a Fledge source tree
- **FLEDGE_INCLUDE** sets the path to Fledge header files
- **FLEDGE_LIB sets** the path to Fledge libraries
- **FLEDGE_INSTALL** sets the installation path of Random plugin

NOTE:
 - The **FLEDGE_INCLUDE** option should point to a location where all the Fledge 
   header files have been installed in a single directory.
 - The **FLEDGE_LIB** option should point to a location where all the Fledge
   libraries have been installed in a single directory.
 - 'make install' target is defined only when **FLEDGE_INSTALL** is set

Examples:

- no options

  $ cmake ..

- no options and FLEDGE_ROOT set

  $ export FLEDGE_ROOT=/some_fledge_setup

  $ cmake ..

- set FLEDGE_SRC

  $ cmake -DFLEDGE_SRC=/home/source/develop/Fledge  ..

- set FLEDGE_INCLUDE

  $ cmake -DFLEDGE_INCLUDE=/dev-package/include ..
- set FLEDGE_LIB

  $ cmake -DFLEDGE_LIB=/home/dev/package/lib ..
- set FLEDGE_INSTALL

  $ cmake -DFLEDGE_INSTALL=/home/source/develop/Fledge ..

  $ cmake -DFLEDGE_INSTALL=/usr/local/fledge ..
