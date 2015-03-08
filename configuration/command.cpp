/**
 * \file command.cpp
 * \brief Command interpreter
 *
 * Command interpreter.
 *
 * prefix: command
 *
 * \author grochu
 * \date 2012-08-30
 */

#include <list>

#include <cstdint>
#include <cstddef>
#include <cstring>

#include "command.h"
#include "error.h"
#include "config.h"

#include "FreeRTOS.h"

/*---------------------------------------------------------------------------------------------------------------------+
| local functions' prototypes
+---------------------------------------------------------------------------------------------------------------------*/

static enum Error _helpHandler(const char **arguments_array, uint32_t arguments_count, char *output_buffer,
		size_t output_buffer_length);

/*---------------------------------------------------------------------------------------------------------------------+
| local variables
+---------------------------------------------------------------------------------------------------------------------*/

static const struct CommandDefinition _helpCommandDefinition =
{
		"help",								// command string
		0,									// maximum number of arguments
		_helpHandler,						// callback function
		"help: lists all available commands\r\n",	// string displayed by help function
};

std::list<const CommandDefinition*> _commands = {&_helpCommandDefinition};

/*---------------------------------------------------------------------------------------------------------------------+
| global functions
+---------------------------------------------------------------------------------------------------------------------*/

/**
 * \brief Converts arguments to map of key-value pairs.
 *
 * Converts arguments to map of key-value pairs. Key is identified by double hyphen. Values are optional.
 *
 * \param [in] arguments_array is an array with arguments
 * \param [in] arguments_count is the number or arguments on the arguments_array
 * \param [out] pairs is the output array with pairs
 * \param [in] pairs_length is the size of externally provided pairs array
 *
 * \return number of found pairs
 */

uint32_t commandArgumentsToPairs(const char **arguments_array, uint32_t arguments_count,
		struct CommandArgumentPair *pairs, size_t pairs_length)
{
	pairs[0].key = NULL;

	uint32_t i = 0, j = 0;
	uint32_t found = 0;

	while (i < arguments_count && j < pairs_length)
	{
		const char *argument = arguments_array[i++];

		// if the argument is in form --something - it's a key
		if (argument[0] == '-' && argument[1] == '-' && argument[2] != '\0')
		{
			// if current key in pairs array is valid and another key was found - increment the index
			if (pairs[j].key != NULL)
				j++;

			pairs[j].key = &argument[2];
			pairs[j].value = NULL;			// initially value is unknown

			found++;
		}
		else								// otherwise it's a value
			pairs[j].value = argument;
	}

	return found;
}

/**
 * \brief Processes command input string.
 *
 * Processes command input string.
 *
 * \param [in] input is input string
 * \param [out] output_buffer is the pointer to output buffer
 * \param [in] output_buffer_length is the size of output buffer
 *
 * \return ERROR_NONE on success, otherwise an error code defined in the file error.h
 */

enum Error commandProcessInput(char *input, char *output_buffer, size_t output_buffer_length)
{
	*output_buffer = '\0';

	enum Error error = ERROR_COMMAND_NOT_FOUND;

	for (const CommandDefinition* definition : _commands)
	{
		size_t command_length = strlen(definition->command);

		if (strncmp(definition->command, input, command_length) == 0)	// was the input matched to command?
		{
			// always make space for command name - first entry
			uint32_t arguments_count_max = definition->arguments_count_max + 1;

			const char **arguments_array = (const char**)pvPortMalloc(sizeof (char*) * arguments_count_max);

			if (arguments_array == NULL)
				return ERROR_FreeRTOS_errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

			uint32_t arguments_count = 0;
			char *source = input;
			char *lasts;

			while (arguments_count < arguments_count_max)
			{
				char *argument = strtok_r(source, " \t\r\n", &lasts);

				if (argument == NULL)		// valid argument?
					break;					// no - no more arguments available, so break...

				if (strlen(argument) > COMMAND_ARGUMENT_LENGTH)	// should this argument be trimmed?
					argument[COMMAND_ARGUMENT_LENGTH] = '\0';

				arguments_array[arguments_count] = argument;

				arguments_count++;
				source = NULL;
			}

			// execute callback function
			error = (*definition->callback)(arguments_array, arguments_count, output_buffer, output_buffer_length);

			vPortFree(arguments_array);

			if (error != ERROR_NONE)
				return error;

			break;
		}
	}

	return error;
}

/**
 * \brief Registers new command to command interpreter.
 * Registers new command to command interpreter. This function should be called
 * BEFORE the scheduler is started, as it's not thread-safe.
 *
 * \param [in] definition is the pointer to command definition struct, it should
 * be in flash or available through entire program, as only the pointer is
 * copied
 */

void commandRegister(const struct CommandDefinition *definition)
{
	_commands.push_back(definition);
}

/*---------------------------------------------------------------------------------------------------------------------+
| local functions
+---------------------------------------------------------------------------------------------------------------------*/

/**
 * \brief Handler of 'help' command.
 *
 * Handler of 'help' command. Displays all available commands' help strings.
 *
 * \param [out] output_buffer is the pointer to output buffer
 * \param [in] output_buffer_length is the size of output buffer
 *
 * \return ERROR_NONE on success, otherwise an error code defined in the file error.h
 */

static enum Error _helpHandler(const char **arguments_array, uint32_t arguments_count, char *output_buffer,
		size_t output_buffer_length)
{
	(void)arguments_array;					// suppress warning
	(void)arguments_count;					// suppress warning

	for (const CommandDefinition *definition : _commands)
	{
		size_t string_length = strlen(definition->help_string);

		if ((output_buffer_length - 1) < string_length)	// will the help string fit into buffer?
			return ERROR_BUFFER_OVERFLOW;

		memcpy(output_buffer, definition->help_string, string_length);
		output_buffer += string_length;
	}

	*output_buffer = '\0';					// add trailing '\0' to the end of output

	return ERROR_NONE;
}
