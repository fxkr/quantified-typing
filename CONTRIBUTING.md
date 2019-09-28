# Contributing


## Design thoughts

I went for a traditional thread-based approach. I do not consider this optimal,
but I have gotten used to single-threaded, event-driven designs (typically based
on libraries around epoll, like uloop or libevent) and wanted to try something
different for once. It is nice in that it makes the code look more straightforward.


## Useful tools

Preliminary note: below commands expect to run from a clean state, e.g.:

```
make clean
```

Include what you use:

```
CC=clang cmake -DCMAKE_C_INCLUDE_WHAT_YOU_USE="$(which iwyu);-Xiwyu;any;-Xiwyu;iwyu;-Xiwyu;args" cmake . && CC=clang make
```

Style check:

```
CC=clang cmake -DCMAKE_C_CLANG_TIDY="clang-tidy;-checks=-*,readability-*" cmake . && CC=clang make
```

Static code analysis:

```
scan-build cmake . && scan-build --view make
```
