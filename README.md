# The MeON Interpreter

[![MIT license](https://img.shields.io/badge/License-MIT-green.svg)](https://lbesson.mit-license.org/)
![Lines of code](https://img.shields.io/tokei/lines/github/heinthanth/meon?label=Line%20of%20Code)

`MeON VM` is the Virtual Machine for `MeON` programming language which has some unique syntax and is designed for beginner in Programming. `MeON VM` can either interpret `MeON` or compile to native executable. It is distributed under [MIT License](LICENSE).

## Documentations

Right now, it's just under development. So, document is haven't written yet. But some example script that I used to test VM are located under [examples](examples/). However, I'll create a website for it!

## Installation

Well, please compile it :3 It's quite simple. You just need `gcc` and `make`.

First, clone the repo.

```shell
git clone https://github.com/heinthanth/meon && cd meon
```

Then, compile it

```shell
make
```

If there's no error, VM is located under [build](build/) and can be executed.

```shell
build/meon --version
```

## Common Questions

Here are some FAQ about `MeON` project.

-   WHY IT'S NAMED `MeON` ?

    -   it's from **Melon** and then, I removed **L**.
    -   It's like `Neon`, `Xeon` and sounds sweet.

-   WHY THE FU\*K I CREATED THIS ?
    -   No reason! I just want to learn something new by creating something new.
    -   I gain massive knowledge about **How CPU works**, **How machine interpret and execute various instructions and jump to and from**.

## License

The `MeON` Interpreter is licensed under MIT. See [LICENSE](LICENSE) for more details.
