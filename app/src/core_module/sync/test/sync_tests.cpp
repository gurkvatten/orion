
namespace orion::sync::tests {
void payload_test();
void gzip_test();
void gzip_base64_test();
void external_facade_test();
void external_facade_rollback_test();
void export_test();
void import_test();
void cancel_test();

void
sync_tests()
{
    //payload_test();
    //gzip_test();
    //gzip_base64_test();
    //external_facade_test();
    //external_facade_rollback_test();
    export_test();
    import_test();
    //cancel_test();
}

} // namespace orion::sync_tests