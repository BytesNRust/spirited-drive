/* The names TNFS_INPUT_SCRIPT's key table (script_key) understands: the words a scripted walk uses, and the
 * set-1 scancode each is. The page walks need PGUP and PGDN (the BIOS page codes the port's page screens ask
 * for with input_page_keys), and HOME and END come along for the price of a table row. */
#include "platform/platform.h"
#include "sys/page_keys.h"
#include "test.h"

#include <string.h>

int main(void)
{
    /* the names the table already had */
    CHECK_EQ_INT(platform_script_key("UP"), 0x48);
    CHECK_EQ_INT(platform_script_key("DOWN"), 0x50);
    CHECK_EQ_INT(platform_script_key("LEFT"), 0x4B);
    CHECK_EQ_INT(platform_script_key("RIGHT"), 0x4D);
    CHECK_EQ_INT(platform_script_key("ENTER"), 0x1C);
    CHECK_EQ_INT(platform_script_key("ESC"), 0x01);
    CHECK_EQ_INT(platform_script_key("SPACE"), 0x39);

    /* port: the page keys, so a page walk can be scripted by keyboard */
    CHECK_EQ_INT(platform_script_key("PGDN"), 0x51);
    CHECK_EQ_INT(platform_script_key("PGUP"), 0x49);
    CHECK_EQ_INT(platform_script_key("HOME"), 0x47);
    CHECK_EQ_INT(platform_script_key("END"), 0x4F);

    /* a letter and a digit still map to their set-1 scancodes */
    CHECK_EQ_INT(platform_script_key("A"), 0x1E);
    CHECK_EQ_INT(platform_script_key("Z"), 0x2C);
    CHECK_EQ_INT(platform_script_key("1"), 0x02);
    CHECK_EQ_INT(platform_script_key("0"), 0x0B);

    /* a name the table does not have is not a key */
    CHECK_EQ_INT(platform_script_key("PGDOWNN"), -1);
    CHECK_EQ_INT(platform_script_key("pgdn"), -1);
    CHECK_EQ_INT(platform_script_key(""), -1);

    /* the keypad's 1 and End share the port's set-1 code (0x4f): the codes carry no extended prefix */
    CHECK_EQ_INT(platform_script_key("KP1"), 0x4F);
    CHECK_EQ_INT(platform_script_key("END"), platform_script_key("KP1"));

    /* The seam the scripted page walk crosses: the name's scancode is the one page_key reads, so the key the script
     * presses is the key the screen sees. page_key is what input_get_key returns when the page keys are on, and the
     * screens' PAGE_DOWN (0x5100) is what it answers. */
    uint8_t flags[KEY_FLAGS];
    memset(flags, 0, sizeof flags);
    flags[platform_script_key("PGDN")] = 1;
    CHECK_EQ_INT(page_key(flags), 0x5100);
    memset(flags, 0, sizeof flags);
    flags[platform_script_key("PGUP")] = 1;
    CHECK_EQ_INT(page_key(flags), 0x4900);
    return TEST_REPORT();
}
