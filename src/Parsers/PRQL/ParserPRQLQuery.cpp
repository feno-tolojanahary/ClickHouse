#include <string>
#include <Parsers/PRQL/ParserPRQLQuery.h>

#include "config.h"

#if USE_PRQL
#    include <prql.h>
#endif

#include <Parsers/ParserQuery.h>
#include <Parsers/ParserSetQuery.h>
#include <Parsers/parseQuery.h>
#include <base/scope_guard.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int SYNTAX_ERROR;
}

bool ParserPRQLQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserSetQuery set_p;

    if (set_p.parse(pos, node, expected))
    {
        return true;
    }

    uint8_t * sql_query_ptr{nullptr};
    uint64_t sql_query_size{0};
    const auto * begin = pos->begin;

    const auto res
        = prql_to_sql(reinterpret_cast<const uint8_t *>(begin), static_cast<uint64_t>(end - begin), &sql_query_ptr, &sql_query_size);

    SCOPE_EXIT({ prql_free_pointer(sql_query_ptr); });

    const auto * sql_query_char_ptr = reinterpret_cast<char *>(sql_query_ptr);

    if (res != 0)
    {
        throw Exception(ErrorCodes::SYNTAX_ERROR, "PRQL syntax error: '{}'", sql_query_char_ptr);
    }

    ParserQuery query_p(end, false);
    String error_message;
    node = tryParseQuery(query_p, sql_query_char_ptr, sql_query_char_ptr + sql_query_size - 1, error_message, false, "", false, 1000, 1999);

    while (!pos->isEnd()) ++pos;

    if (node)
    {
        return true;
    }


    throw Exception(
        ErrorCodes::SYNTAX_ERROR,
        "Error while parsing the SQL query generated from PRQL query. PRQL Query:'{}'; SQL query: '{}'",
        sql_query_char_ptr,
        std::string(sql_query_char_ptr, sql_query_size));
}

}
