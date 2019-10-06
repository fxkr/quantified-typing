# Contributing


## Design thoughts

Preface: this is a hobby project, where I'm trying to do things a bit
differently than I'd normally do them.

This is the third (and only actually usable/complete) iteration of this tool.
The first version was written a few years back, and it was based on Xlib.
It broke when I moved to Wayland. So the second version was a kernel module.
This is too annoying to actually deploy/maintain.
So now the third version is based on libevdev.

I went for a traditional thread-based approach. I do not consider this optimal,
but I have gotten used to single-threaded, event-driven designs (typically based
on libraries around epoll, like uloop or libevent) and wanted to try something
different for once. It is nice in that it makes the code look more straightforward
(even though of course the appearance is deceiving; in reality this concurrency
based on shared memory and synchronization primitives is unnecessarily complex).


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
