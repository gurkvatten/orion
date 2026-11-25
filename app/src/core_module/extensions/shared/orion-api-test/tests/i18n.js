"use strict";

TestRunner.suite("i18n", (test) => {
    test("getUILanguage returns a language string", async () => {
        const lang = api.i18n.getUILanguage();
        assertType(lang, "string", "getUILanguage()");
        assert(lang.length > 0, "should not be empty");
    });

    test("getMessage with defined key", async () => {
        const msg = api.i18n.getMessage("test_message");
        assertEqual(msg, "Hello from test extension");
    });

    test("getMessage with undefined key returns empty", async () => {
        const msg = api.i18n.getMessage("nonexistent_key_xyz");
        assertEqual(msg, "");
    });

    test("getMessage with @@extension_id", async () => {
        const id = api.i18n.getMessage("@@extension_id");
        assertType(id, "string", "@@extension_id");
        assert(id.length > 0, "@@extension_id should not be empty");
    });

    test("getMessage with @@ui_locale", async () => {
        const locale = api.i18n.getMessage("@@ui_locale");
        assertType(locale, "string", "@@ui_locale");
        assert(locale.length > 0, "@@ui_locale should not be empty");
    });
});
