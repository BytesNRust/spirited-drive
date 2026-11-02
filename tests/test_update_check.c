/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The update check's pure half (core/update/update_check.c): the tag from GitHub's answer and version order. */
#include "update/update_check.h"
#include "test.h"

static void parses_the_tag(void)
{
    char tag[32];
    const char *json = "{\"url\":\"x\",\"tag_name\": \"v0.9.1\",\"name\":\"TNFS Restomod 0.9.1\"}";
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

int main(void)
{
    parses_the_tag();
    rejects_what_is_not_a_tag();
    orders_versions();
    dev_builds_never_ask();
    return TEST_REPORT();
}
