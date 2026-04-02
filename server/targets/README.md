# Primary AFLGo Targets

The primary AFLGo subject in this repository is:

- `build/mini_http_handler`

That binary is built from:

- `server/harness.cpp`
- `server/http_core.cpp`

The intentional crash sites are inside `server/http_core.cpp`, which is the shared parser/handler used by the standalone handler and the optional socket wrapper.

Recommended `BBtargets.txt` entries are the actual crash lines inside these functions:

- `bug_null_deref`
- `bug_divide_by_zero`
- `bug_abort`
- `bug_bad_function_pointer`
- `bug_oob_stack_write`
- `bug_use_after_free`

Use line-based targets like:

```text
server/http_core.cpp:LINE_NUMBER
```

Suggested campaign order:

1. target `bug_null_deref`
2. target `bug_abort`
3. target `bug_oob_stack_write`
4. target `bug_bad_function_pointer`
5. target `bug_use_after_free`

This keeps early campaigns shallow and later campaigns more selective.
