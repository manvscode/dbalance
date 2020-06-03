#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>
#include <xtd/cmdopt.h>
#include <xtd/filesystem.h>
#include <xtd/floating-point.h>
#include <xtd/string.h>
#include <collections/vector.h>
#include <csv.h>

typedef struct position {
	const char* symbol;
	int quantity;
	const char* description;
	double price;
	double yearly_high;
	double yearly_low;
	double dividend_yield;
} position_t;

typedef struct app {
	bool is_yield_target_set;
	int column;
	int row;
	double yield_target;
	position_t* positions;
} app_t;


static bool command_read_input( const cmd_opt_ctx_t* ctx, void* user_data );
static bool command_set_yield_target( const cmd_opt_ctx_t* ctx, void* user_data );
static bool command_show_help( const cmd_opt_ctx_t* ctx, void* user_data );
static void command_error( cmd_opt_result_t error, const char* opt, void* user_data );
static int position_market_value_compare(const void* l, const void* r);
double position_market_value(const position_t* position);
void portfolio_calculate(const position_t* positions, double* portfolio_value, double* portfolio_yield, const char* exclude);

static const cmd_opt_t OPTIONS[] = {
	{ "-i", "--input", 1, "Read a CSV file.", command_read_input },
	{ "-y", "--yield-target", 1, "Suggest changes for a target yield..", command_set_yield_target },
	{ "-h", "--help", 0, "Show the help.", command_show_help },
};
static const size_t OPTIONS_COUNT = sizeof(OPTIONS) / sizeof(OPTIONS[0]);



void csv_parse_field_done(void* f, size_t sz, void* user_data )
{
	app_t* app = (app_t*) user_data;
	char* field = (char*) f;

	if( app->row == 0 )
	{
		return;
	}


	if( app->column == 0 )
	{
		lc_vector_push_emplace( app->positions );
	}

	position_t* position = &lc_vector_last(app->positions);
	char* end = NULL;

	/*
	 * BUG BUG: These are the expected column ordering when
	 * exporting the "Fundamental" view on TD Ameritrade.
	 */
	switch( app->column )
	{
		case 0: // Symbol
			position->symbol = string_dup( field );
			break;
		case 1: // Qty
		{
			//position->quantity = atoi( field );
			int quantity = 0;
			int quantity_multiplier = 1;
			char quantity_multiplier_str[32];
			sscanf(field, "%d%s", &quantity, quantity_multiplier_str);

			if (strcasecmp("M", quantity_multiplier_str) == 0) {
				quantity_multiplier = 10;
			} else if (strcasecmp("MM", quantity_multiplier_str) == 0) {
				quantity_multiplier = 100;
			} else {
				quantity_multiplier = 1;
			}

			position->quantity = quantity * quantity_multiplier;
			quantity = 0;
			quantity_multiplier_str[0] = '\0';
			break;
		}
		case 2: // Description
			position->description = string_dup( field );
			break;
		case 3: // Price
			position->price = strtod( field, &end );
			break;
		case 5: // 52-week high
			position->yearly_high = strtod( field, &end );
			break;
		case 6: // 52-week low
			position->yearly_low = strtod( field, &end );
			break;
		case 8: // Div yield
		{
			char* percent = strchr( field, '%' );
			if (percent ) *percent = '\0';
			position->dividend_yield = strtod( field, &end ) / 100.0;
			break;
		}
		// Ignore -------------------------
		case 4: // Chg ($)
		case 7: // P/E
		case 9: // Ex-div date
		default:
			break;
	}

	app->column += 1;

	//printf("%s\t", field );
}
void csv_parse_row_done( int row, void* user_data )
{
	app_t* app = (app_t*) user_data;
	app->column = 0;
	app->row += 1;
}

bool command_read_input( const cmd_opt_ctx_t* ctx, void* user_data )
{
	bool result = true;
	app_t* app = (app_t*) user_data;
	struct csv_parser parser;

	const char** arguments = cmd_opt_args( ctx );
	const char* filename = arguments[0];

	if( !file_exists(filename) )
	{
		fprintf( stderr, "ERROR: " );
		fprintf( stderr, "File \"%s\" does not exist.\n", filename );
		result = false;
	}

	if( csv_init( &parser, CSV_STRICT | CSV_APPEND_NULL ) != 0)
	{
		fprintf( stderr, "ERROR: " );
		fprintf( stderr, "Failed to initialize csv parser\n" );
		result = false;
		goto done;
	}

#if 1
	csv_set_delim( &parser, CSV_TAB);
#else
	csv_set_delim( &parser, CSV_COMMA);
#endif

	FILE* file = fopen( filename, "rb" );

	if( !file )
	{
		fprintf( stderr, "ERROR: " );
		fprintf( stderr, "Unable to open %s\n", filename );
		result = false;
		goto done;
	}

	char buffer[256];
	size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);

	while( bytes_read > 0 )
	{
		size_t bytes_parsed = csv_parse( &parser, buffer, bytes_read, csv_parse_field_done, csv_parse_row_done, app );
		if( bytes_parsed != bytes_read )
		{
			fprintf( stderr, "ERROR: " );
			fprintf( stderr, "%s\n", csv_strerror(csv_error(&parser)) );
			result = false;
			goto done;
		}
		bytes_read = fread(buffer, 1, sizeof(buffer), file);
	}

	csv_fini( &parser, csv_parse_field_done, csv_parse_row_done, app );

done:
	csv_free( &parser );
	if( file ) fclose( file );
	return result;
}

bool command_set_yield_target( const cmd_opt_ctx_t* ctx, void* user_data )
{
	app_t* app = (app_t*) user_data;
	size_t arg_count = cmd_opt_args_count( ctx );
	const char** arguments = cmd_opt_args( ctx );
	const char* yield_target_str = arguments[0];

	app->yield_target = atof(yield_target_str) / 100.0;
	app->is_yield_target_set = true;

	return arg_count == 1;
}

bool command_foobar( const cmd_opt_ctx_t* ctx, void* user_data )
{
	return true;
}

bool command_show_help( const cmd_opt_ctx_t* ctx, void* user_data )
{
	printf("Dividend Portfolio Balancer\n");
	printf("Copyright (c) 2020, Joe Marrero. All rights reserved.\n\n");
	printf("Command-line Options:\n");
	cmd_opt_print_help( OPTIONS, OPTIONS_COUNT );
	printf("\n");
	return false;
}

void command_error( cmd_opt_result_t error, const char* opt, void* user_data )
{
	switch( error )
	{
		case CMD_OPT_ERR_NO_OPTIONS:
			fprintf( stderr, "ERROR: " );
			fprintf( stderr, "Need at least one option.\n" );
			break;
		case CMD_OPT_ERR_UNEXPECTED_OPTION:
			fprintf( stderr, "ERROR: " );
			fprintf( stderr, "Unexpected option \"%s\"\n", opt );
			break;
		case CMD_OPT_ERR_INVALID_ARG_COUNT:
		{
			fprintf( stderr, "ERROR: " );

			const cmd_opt_t* option = cmd_opt_find( OPTIONS, OPTIONS_COUNT, opt );
			if( option->opt_arg_count > 1 )
			{
				fprintf( stderr, "Expected %d arguments for \"%s\" but encounted less.\n",
						option->opt_arg_count, opt );

			}
			else
			{
				fprintf( stderr, "Expected %d argument for \"%s\" but encounted none.\n",
						option->opt_arg_count, opt );
			}
			break;
		}
		//case CMD_OPT_ERR_ABORTED:
			//fprintf( stderr, "ERROR: " );
			//fprintf( stderr, "Aborted handling \"%s\"\n", opt );
			//break;
		default:
			break;
	}
}

int position_market_value_compare(const void* l, const void* r)
{
	const position_t* left = l;
	const position_t* right = r;
	return (int) 100 * (position_market_value(right) - position_market_value(left));
}

int position_yield_compare(const void* l, const void* r)
{
	const position_t* left = l;
	const position_t* right = r;
	return (int) 100 * (right->dividend_yield - left->dividend_yield);
}


double position_market_value(const position_t* position)
{
	return position->quantity * position->price;
}


void portfolio_calculate(const position_t* positions, double* portfolio_value, double* portfolio_yield, const char* exclude)
{
	*portfolio_value = 0.0;
	*portfolio_yield = 0.0;

	for( int i = 0; i < lc_vector_size(positions); i++)
	{
		const position_t* position = &positions[i];
		bool is_excluded = (exclude && strcmp(position->symbol, exclude) == 0);

		if (!is_excluded)
		{
			*portfolio_value += position_market_value(position);
		}
	}

	for( int i = 0; i < lc_vector_size(positions); i++)
	{
		const position_t* position = &positions[i];
		bool is_excluded = (exclude && strcmp(position->symbol, exclude) == 0);

		if (!is_excluded)
		{
			double weight = position_market_value(position) / *portfolio_value;

			if( !double_is_equal(position->dividend_yield, 0.0) )
			{
				*portfolio_yield += weight * position->dividend_yield;
			}
		}
	}
}



int main( int argc, char* argv[] )
{
#ifdef HAVE_LIBCSV
	//fprintf( stdout, "Using libcsv\n" );
#endif
	setlocale(LC_NUMERIC, "");

	app_t app = {
		.yield_target = 0.05,
		.positions = NULL,
	};
	int exit_status = 0;
	lc_vector_create( app.positions, 1 );

	cmd_opt_result_t cmd_result = cmd_opt_process( argc, argv,
			OPTIONS, OPTIONS_COUNT,
			command_error, command_foobar,
		   	&app );

	if( cmd_result < 0 )
	{
		exit_status = -1;
		goto done;
	}


	double portfolio_value = 0.0;
	double portfolio_yield = 0.0;

	portfolio_calculate(app.positions, &portfolio_value, &portfolio_yield, NULL);


	// Sort by yield
	qsort(app.positions, lc_vector_size(app.positions), sizeof(position_t), position_yield_compare);

	// Sort by market value
	//qsort(app.positions, lc_vector_size(app.positions), sizeof(position_t), position_market_value_compare);

	printf("Current portfolio:\n\n");


	printf("%10s %-6s $%-7s %7s%%  $%-10s\n", "Symbol", "Qty", "Price", "Yield", "Mkt. Value" );
	printf("  ---------------------------------------------------------\n");
	for( int i = 0; i < lc_vector_size(app.positions); i++)
	{
		const position_t* position = &app.positions[i];
		printf("%10.10s %-6d $%-'7.2lf %7.2lf%%  $%-'10.2lf\n", position->symbol, position->quantity, position->price, 100 * position->dividend_yield, position_market_value(position));
	}
	printf("  ---------------------------------------------------------\n");


	printf("  Portfolio Positions: %ld\n", lc_vector_size(app.positions));
	printf("  Portfolio Value: $%'.2lf\n", portfolio_value);
	printf("  Portfolio Yield: %.3lf%%\n", 100.0 * portfolio_yield);
	const double yearly_income = portfolio_yield * portfolio_value;
	printf("  Expected Income: $%'.2lf per year, or $%'.2lf per month\n", yearly_income, yearly_income / 12.0 );
	printf("  ---------------------------------------------------------\n");


	if (app.is_yield_target_set)
	{
		printf("\n\n");
		printf("For a target Yield of %.3f%%, we can make the following changes:\n\n", 100 * app.yield_target);

		printf("%10s %-6s %-15s\n", "Symbol", "Qty", "Investment");
		printf("  ----------------------------\n");
		for( int i = 0; i < lc_vector_size(app.positions); i++)
		{
			const position_t* position = &app.positions[i];

			double filtered_portfolio_value = 0.0;
			double filtered_portfolio_yield = 0.0;

			portfolio_calculate(app.positions, &filtered_portfolio_value, &filtered_portfolio_yield, position->symbol);

			const double target_position_value = (app.yield_target * filtered_portfolio_value - filtered_portfolio_value * filtered_portfolio_yield) / (position->dividend_yield - app.yield_target);
			const int target_shares = target_position_value / position->price;

			const double investment_amount = target_position_value - position_market_value(position);
			const int change_quantity = target_shares - position->quantity;

			if (change_quantity != 0 && (position->quantity + change_quantity) >= 0) { // Make sure the change is possible.
				printf("%10.10s %-6d $%-'15.2lf\n", position->symbol, change_quantity, investment_amount);
			}
		}
		printf("  ----------------------------\n");
	}

done:
	lc_vector_destroy( app.positions );
	return 0;
}
