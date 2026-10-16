// nimcp_logging_part_core.c - core functions
// Part of nimcp_logging.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_logging.c


void log_message(log_level_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    nimcp_log_writev(NULL, level, NULL, NULL, 0, format, args);
    va_end(args);
}


void nimcp_log(log_level_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    nimcp_log_writev(NULL, level, NULL, NULL, 0, format, args);
    va_end(args);
}
