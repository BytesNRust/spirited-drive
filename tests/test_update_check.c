/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The update check's pure half (core/update/update_check.c): the tag from GitHub's answer and version order. */
#include "update/update_check.h"
#include "test.h"

static void parses_the_tag(void)
{
    char tag[32];
    const char *json = "{\"url\":\"x\",\"tag_name\": \"v0.9.1\",\"name\":\"Spirited Drive 0.9.1\"}";
    CHECK_EQ_INT(update_parse_latest_tag(json, tag, sizeof tag), 1);
    CHECK(strcmp(tag, "0.9.1") == 0);

    CHECK_EQ_INT(update_parse_latest_tag("{\"tag_name\":\"1.0.0-beta.2\"}", tag, sizeof tag), 1);
    CHECK(strcmp(tag, "1.0.0-beta.2") == 0);
}

static void rejects_what_is_not_a_tag(void)
{
    char tag[8];
    CHECK_EQ_INT(update_parse_latest_tag(NULL, tag, sizeof tag), 0);
    CHECK_EQ_INT(update_parse_latest_tag("{\"message\":\"Not Found\"}", tag, sizeof tag), 0);
    CHECK_EQ_INT(update_parse_latest_tag("{\"tag_name\":\"\"}", tag, sizeof tag), 0);
    CHECK_EQ_INT(update_parse_latest_tag("{\"tag_name\":\"v1.0.0; rm\"}", tag, sizeof tag), 0);
    CHECK_EQ_INT(update_parse_latest_tag("{\"tag_name\":\"v1.0.0-a-very-long-one\"}", tag, sizeof tag), 0);
    CHECK(tag[0] == '\0');
    CHECK_EQ_INT(update_parse_latest_tag("{\"tag_name\":\"v1.0.0", tag, sizeof tag), 0);
}

static void orders_versions(void)
{
    CHECK(update_version_compare("0.9.0", "0.9.1") < 0);
    CHECK(update_version_compare("0.10.0", "0.9.9") > 0);
    CHECK(update_version_compare("1.0.0", "1.0.0") == 0);
    CHECK(update_version_compare("v1.0.0", "1.0.0") == 0);
    CHECK(update_version_compare("1.0.0-beta.2", "1.0.0") < 0);
    CHECK(update_version_compare("1.0.0-beta.2", "1.0.0-beta.10") < 0);
    CHECK(update_version_compare("1.0.0-alpha", "1.0.0-beta") < 0);
    CHECK(update_version_compare("1.0.0-1", "1.0.0-alpha") < 0);
    CHECK(update_version_compare("1.0.0-beta", "1.0.0-beta.1") < 0);
    CHECK(update_version_compare("0.9.0+abc", "0.9.0") == 0);
    CHECK(update_version_compare("garbage", "0.9.0") == 0);
}

static void dev_builds_never_ask(void)
{
    CHECK_EQ_INT(update_is_dev_build("0.9.0-dev+1efe883c"), 1);
    CHECK_EQ_INT(update_is_dev_build("0.9.0-dev"), 1);
    CHECK_EQ_INT(update_is_dev_build("0.9.0"), 0);
    CHECK_EQ_INT(update_is_dev_build("0.9.1-beta.1"), 0);
    CHECK_EQ_INT(update_is_dev_build("nonsense"), 1);
}

static void api_parse_reads_latest(void)
{
    char out[40];
    CHECK_EQ_INT(update_parse_api_latest("{\"latest\":\"0.9.1\",\"page\":\"x\"}", out, sizeof out), 1);
    CHECK(strcmp(out, "0.9.1") == 0);
    CHECK_EQ_INT(update_parse_api_latest("{ \"page\": \"x\", \"latest\" : \"1.0.0-beta.2\" }", out, sizeof out), 1);
    CHECK(strcmp(out, "1.0.0-beta.2") == 0);
}

static void api_parse_rejects_garbage(void)
{
    char out[40] = "stale";
    CHECK_EQ_INT(update_parse_api_latest(NULL, out, sizeof out), 0);
    CHECK_EQ_INT(out[0], 0);
    CHECK_EQ_INT(update_parse_api_latest("<html><body>503</body></html>", out, sizeof out), 0);
    CHECK_EQ_INT(update_parse_api_latest("{\"latest\":null}", out, sizeof out), 0);   /* before the first release: 503 {"latest":null} */
    CHECK_EQ_INT(update_parse_api_latest("{\"latest\":\"\"}", out, sizeof out), 0);
    CHECK_EQ_INT(update_parse_api_latest("{\"latest\":\"1.0.0; rm\"}", out, sizeof out), 0);
    CHECK_EQ_INT(update_parse_api_latest("{\"latest\":\"v0.9.1\"}", out, sizeof out), 0);   /* the API gives it plain */
    CHECK_EQ_INT(update_parse_api_latest("{\"latest\":\"0.9\"}", out, sizeof out), 0);
    CHECK_EQ_INT(update_parse_api_latest("{\"latest\":\"1.2.3-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}", out, sizeof out), 0);   /* 33+ chars */
    CHECK_EQ_INT(out[0], 0);
    CHECK_EQ_INT(update_parse_api_latest("{\"latest\":\"0.9.1\"}", out, 4), 0);   /* does not fit */
    CHECK_EQ_INT(update_parse_api_latest("{\"latest\":\"0.9.1\"}", out, 0), 0);
}

static void api_version_loses_the_build(void)
{
    char v[40];
    CHECK_EQ_INT(update_api_version("0.9.0-beta.1+abc", v, sizeof v), 1);
    CHECK(strcmp(v, "0.9.0-beta.1") == 0);
    CHECK_EQ_INT(update_api_version("0.9.0", v, sizeof v), 1);
    CHECK(strcmp(v, "0.9.0") == 0);
    CHECK_EQ_INT(update_api_version("0.9.0-dev+1efe883c", v, sizeof v), 1);
    CHECK(strcmp(v, "0.9.0-dev") == 0);
    CHECK_EQ_INT(update_api_version("v0.9.0", v, sizeof v), 0);
    CHECK_EQ_INT(update_api_version("0.9.0; rm -rf /", v, sizeof v), 0);
    CHECK_EQ_INT(update_api_version("0.9.0\"", v, sizeof v), 0);
    CHECK_EQ_INT(update_api_version("", v, sizeof v), 0);
    CHECK_EQ_INT(update_api_version(NULL, v, sizeof v), 0);
    CHECK_EQ_INT(update_api_version("1.2.3-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", v, sizeof v), 0);   /* over 32 */
}

static void api_commands_are_bounded(void)
{
    char cmd[512];
    CHECK_EQ_INT(update_api_command(cmd, sizeof cmd, NULL, "0.9.0-beta.1+abc"), 1);
    CHECK(strstr(cmd, "\"" UPDATE_API_BASE "/latest?v=0.9.0-beta.1\"") != NULL);
    CHECK(strstr(cmd, "--max-time 5") != NULL);
    CHECK(strstr(cmd, "--proto =https") != NULL);
    CHECK(strstr(cmd, "+abc") == NULL);
    CHECK(strstr(cmd, "POST") == NULL);
    CHECK_EQ_INT(update_api_command(cmd, sizeof cmd, "first_race", "0.9.0"), 1);
    CHECK(strstr(cmd, "-X POST") != NULL);
    CHECK(strstr(cmd, "/event?e=first_race&v=0.9.0\"") != NULL);
    CHECK(strstr(cmd, "--max-time 5") != NULL);
    CHECK(strstr(cmd, "--proto =https") != NULL);
    CHECK_EQ_INT(update_api_command(cmd, sizeof cmd, "session", "0.9.0"), 0);          /* only the three */
    CHECK_EQ_INT(update_api_command(cmd, sizeof cmd, "first_race\"; x", "0.9.0"), 0);
    CHECK_EQ_INT(update_api_command(cmd, sizeof cmd, "first_race", "0.9.0;ls"), 0);
    CHECK_EQ_INT(update_api_command(cmd, 40, "first_race", "0.9.0"), 0);                /* does not fit */
}

int main(void)
{
    api_parse_reads_latest();
    api_parse_rejects_garbage();
    api_version_loses_the_build();
    api_commands_are_bounded();
    parses_the_tag();
    rejects_what_is_not_a_tag();
    orders_versions();
    dev_builds_never_ask();
    return TEST_REPORT();
}
