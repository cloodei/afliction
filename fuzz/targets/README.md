# AFLGo Target Notes

The intentional crash sites live in `fuzz/bugs.cpp`.

Typical AFLGo usage is to select one or a few target lines and place them in `BBtargets.txt`:

```text
fuzz/bugs.cpp:LINE_NUMBER
```

Recommended targets are the actual dereference or crash-call lines inside:

- `bug_null_deref`
- `bug_divide_by_zero`
- `bug_abort`
- `bug_bad_function_pointer`
- `bug_oob_stack_write`
- `bug_use_after_free`

Because these lines are stable and isolated, they are good directed-fuzzing targets.

Example strategy:

1. Start with a single target line from `bug_null_deref`.
2. Run AFLGo until it finds a crash.
3. Switch to a deeper target like `bug_bad_function_pointer` or `bug_use_after_free`.

This keeps each campaign focused and easier to interpret.
