var programming_lang_java =
[
    [ "Getting Started with the API  in Java", "basic_api_lang_java.html", [
      [ "Connecting to a database", "basic_api_lang_java.html#basic_connection_lang_java", null ],
      [ "Creating a table", "basic_api_lang_java.html#basic_create_table_lang_java", null ],
      [ "Accessing data with cursors", "basic_api_lang_java.html#basic_cursors_lang_java", null ],
      [ "Closing handles", "basic_api_lang_java.html#basic_close_lang_java", null ]
    ] ],
    [ "Configuration Strings  in Java", "config_strings_lang_java.html", [
      [ "Introduction", "config_strings_lang_java.html#config_intro_lang_java", null ],
      [ "JavaScript Object Notation (JSON) compatibility", "config_strings_lang_java.html#config_json_lang_java", null ]
    ] ],
    [ "Cursors  in Java", "cursors_lang_java.html", "cursors_lang_java" ],
    [ "Transactions  in Java", "transactions_lang_java.html", [
      [ "ACID properties", "transactions_lang_java.html#transactions_acid_lang_java", null ],
      [ "Transactional API", "transactions_lang_java.html#transactions_api_lang_java", null ],
      [ "Implicit transactions", "transactions_lang_java.html#transactions_implicit_lang_java", null ],
      [ "Concurrency control", "transactions_lang_java.html#transactions_concurrency_lang_java", null ],
      [ "Isolation levels", "transactions_lang_java.html#transaction_isolation_lang_java", null ],
      [ "Named Snapshots", "transactions_lang_java.html#transaction_named_snapshots_lang_java", null ],
      [ "Application-specified Transaction Timestamps", "transactions_lang_java.html#transaction_timestamps_lang_java", [
        [ "support in the extension API", "transactions_lang_java.html#Timestamp_lang_java", null ]
      ] ]
    ] ],
    [ "Error handling  in Java", "error_handling_lang_java.html", [
      [ "Translating errors", "error_handling_lang_java.html#error_translation_lang_java", null ]
    ] ],
    [ "Schema, Columns, Column Groups, Indices and Projections  in Java", "schema_lang_java.html", "schema_lang_java" ],
    [ "Log-Structured Merge Trees  in Java", "lsm_lang_java.html", [
      [ "Background", "lsm_lang_java.html#lsm_background_lang_java", null ],
      [ "Description of LSM trees", "lsm_lang_java.html#lsm_description_lang_java", null ],
      [ "Interface to LSM trees", "lsm_lang_java.html#lsm_api_lang_java", null ],
      [ "Merging", "lsm_lang_java.html#lsm_merge_lang_java", null ],
      [ "Bloom filters", "lsm_lang_java.html#lsm_bloom_lang_java", null ],
      [ "Creating tables using LSM trees", "lsm_lang_java.html#lsm_schema_lang_java", null ],
      [ "Caveats", "lsm_lang_java.html#lsm_caveats_lang_java", [
        [ "Key_format configuration", "lsm_lang_java.html#lsm_key_format_lang_java", null ],
        [ "Named checkpoints", "lsm_lang_java.html#lsm_checkpoints_lang_java", null ]
      ] ]
    ] ],
    [ "File formats and compression  in Java", "file_formats_lang_java.html", "file_formats_lang_java" ],
    [ "Compressors  in Java", "compression_lang_java.html", [
      [ "Using LZ4 compression", "compression_lang_java.html#compression_lz4_lang_java", null ],
      [ "Using snappy compression", "compression_lang_java.html#compression_snappy_lang_java", null ],
      [ "Using zlib compression", "compression_lang_java.html#compression_zlib_lang_java", null ],
      [ "Using Zstd compression", "compression_lang_java.html#compression_zstd_lang_java", null ],
      [ "Upgrading compression engines", "compression_lang_java.html#compression_upgrading_lang_java", null ],
      [ "Custom compression engines", "compression_lang_java.html#compression_custom_lang_java", null ]
    ] ],
    [ "Encryptors  in Java", "encryption_lang_java.html", "encryption_lang_java" ],
    [ "Multithreading  in Java", "threads_lang_java.html", [
      [ "Code samples", "threads_lang_java.html#threads_example_lang_java", null ]
    ] ],
    [ "Name spaces  in Java", "namespace_lang_java.html", [
      [ "Process' environment name space", "namespace_lang_java.html#namespace_env_lang_java", null ],
      [ "Java language name space", "namespace_lang_java.html#namespace_c_lang_java", null ],
      [ "File system name space", "namespace_lang_java.html#namespace_filesystem_lang_java", null ]
    ] ],
    [ "Database read-only mode  in Java", "readonly_lang_java.html", [
      [ "Database read-only configuration considerations", "readonly_lang_java.html#readonly_config_lang_java", null ],
      [ "Readonly configuration and recovery", "readonly_lang_java.html#readonly_recovery_lang_java", null ],
      [ "Readonly configuration and logging", "readonly_lang_java.html#readonly_logging_lang_java", null ],
      [ "Readonly configuration and LSM trees", "readonly_lang_java.html#readonly_lsm_lang_java", null ],
      [ "Readonly configuration and multiple database handles", "readonly_lang_java.html#readonly_handles_lang_java", null ]
    ] ],
    [ "Asynchronous operations  in Java", "async_lang_java.html", [
      [ "Configuring asynchronous operations", "async_lang_java.html#async_config_lang_java", null ],
      [ "Allocating an asynchronous operations handle", "async_lang_java.html#async_alloc_lang_java", null ],
      [ "Executing asynchronous operations", "async_lang_java.html#async_operations_lang_java", null ],
      [ "Waiting for outstanding operations to complete", "async_lang_java.html#async_flush_lang_java", null ],
      [ "Asynchronous operations and transactions", "async_lang_java.html#async_transactions_lang_java", null ]
    ] ],
    [ "Backups  in Java", "backup_lang_java.html", [
      [ "Backup from an application", "backup_lang_java.html#backup_process_lang_java", null ],
      [ "Backup from the command line", "backup_lang_java.html#backup_util_lang_java", null ],
      [ "Incremental backup", "backup_lang_java.html#backup_incremental_lang_java", null ],
      [ "Backup and O_DIRECT", "backup_lang_java.html#backup_o_direct_lang_java", null ]
    ] ],
    [ "Compaction  in Java", "compact_lang_java.html", null ],
    [ "Checkpoint durability  in Java", "checkpoint_lang_java.html", [
      [ "Automatic checkpoints", "checkpoint_lang_java.html#checkpoint_server_lang_java", null ],
      [ "Checkpoint cursors", "checkpoint_lang_java.html#checkpoint_cursors_lang_java", null ],
      [ "Checkpoint naming", "checkpoint_lang_java.html#checkpoint_naming_lang_java", null ],
      [ "Checkpoints and file compaction", "checkpoint_lang_java.html#checkpoint_compaction_lang_java", null ]
    ] ],
    [ "Commit-level durability  in Java", "durability_lang_java.html", [
      [ "Checkpoints", "durability_lang_java.html#durability_checkpoint_lang_java", null ],
      [ "Backups", "durability_lang_java.html#durability_backup_lang_java", null ],
      [ "Bulk loads", "durability_lang_java.html#durability_bulk_lang_java", null ],
      [ "Log file archival", "durability_lang_java.html#durability_archiving_lang_java", null ],
      [ "Tuning commit-level durability", "durability_lang_java.html#durability_tuning_lang_java", [
        [ "Group commit", "durability_lang_java.html#durability_group_commit_lang_java", null ],
        [ "Flush call configuration", "durability_lang_java.html#durability_flush_config_lang_java", null ]
      ] ]
    ] ],
    [ "In-memory databases  in Java", "in_memory_lang_java.html", null ],
    [ "Join cursors  in Java", "cursor_join_lang_java.html", null ],
    [ "Log cursors  in Java", "cursor_log_lang_java.html", null ],
    [ "Rebalance  in Java", "rebalance_lang_java.html", null ],
    [ "Per-process shared caches  in Java", "shared_cache_lang_java.html", null ],
    [ "Statistics  in Java", "statistics_lang_java.html", [
      [ "Statistics logging", "statistics_lang_java.html#statistics_log_lang_java", null ]
    ] ],
    [ "Upgrading and downgrading databases", "upgrade.html", null ],
    [ "Performance monitoring with statistics", "tune_statistics.html", null ],
    [ "Simulating workloads with wtperf", "wtperf.html", [
      [ "Monitoring wtperf", "wtperf.html#monitor", null ],
      [ "Wtperf configuration options", "wtperf.html#config", null ]
    ] ],
    [ "gcc/clang build options", "tune_build_options.html", null ],
    [ "Bulk-load", "tune_bulk_load.html", null ],
    [ "Cache and eviction tuning", "tune_cache.html", [
      [ "Cache size", "tune_cache.html#tuning_cache_size", null ],
      [ "Cache resident objects", "tune_cache.html#tuning_cache_resident", null ],
      [ "Eviction tuning", "tune_cache.html#cache_eviction", null ]
    ] ],
    [ "Checksums", "tune_checksum.html", null ],
    [ "Connection close", "tune_close.html", null ],
    [ "Cursor persistence", "tune_cursor_persist.html", null ],
    [ "Commit-level durability", "tune_durability.html", [
      [ "Group commit", "tune_durability.html#tune_durability_group_commit", null ],
      [ "Flush call configuration", "tune_durability.html#tune_durability_flush_config", null ]
    ] ],
    [ "File allocation", "tune_file_alloc.html", [
      [ "File growth", "tune_file_alloc.html#tuning_system_file_block_grow", null ],
      [ "File block allocation", "tune_file_alloc.html#tuning_system_file_block_allocation", null ]
    ] ],
    [ "Memory allocator", "tune_memory_allocator.html", null ],
    [ "Mutexes", "tune_mutex.html", null ],
    [ "Tuning page size and compression", "tune_page_size_and_comp.html", [
      [ "Data life cycle", "tune_page_size_and_comp.html#data_life_cycle", null ],
      [ "Configurable page structures in WiredTiger", "tune_page_size_and_comp.html#configurable_page_struct", [
        [ "memory_page_max", "tune_page_size_and_comp.html#memory_page_max", null ],
        [ "internal_page_max", "tune_page_size_and_comp.html#internal_page_max", null ],
        [ "leaf_page_max", "tune_page_size_and_comp.html#leaf_page_max", null ],
        [ "allocation_size", "tune_page_size_and_comp.html#allocation_size", null ],
        [ "internal/leaf key/value max", "tune_page_size_and_comp.html#key_val_max", null ],
        [ "split_pct (split percentage)", "tune_page_size_and_comp.html#split_pct", null ]
      ] ],
      [ "Compression considerations", "tune_page_size_and_comp.html#compression_considerations", [
        [ "Table summarizing compression in WiredTiger", "tune_page_size_and_comp.html#table_compress", null ]
      ] ]
    ] ],
    [ "Read-only objects", "tune_read_only.html", null ],
    [ "System buffer cache", "tune_system_buffer_cache.html", [
      [ "Direct I/O", "tune_system_buffer_cache.html#tuning_system_buffer_cache_direct_io", null ],
      [ "os_cache_dirty_max", "tune_system_buffer_cache.html#tuning_system_buffer_cache_os_cache_dirty_max", null ],
      [ "os_cache_max", "tune_system_buffer_cache.html#tuning_system_buffer_cache_os_cache_max", null ]
    ] ],
    [ "Linux transparent huge pages", "tune_transparent_huge_pages.html", null ],
    [ "Linux zone reclamation memory management", "tune_zone_reclaim.html", null ]
];