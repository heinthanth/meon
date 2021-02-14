# The Meon Interpreter

[![MIT license](https://img.shields.io/badge/License-MIT-green.svg)](https://lbesson.mit-license.org/)
![Lines of code](https://img.shields.io/tokei/lines/github/heinthanth/meon?label=Line%20of%20Code)

`Meon VM` is the Virtual Machine for `Meon` programming language which has some unique syntax and is designed for beginner in Programming. `Meon VM` can either interpret `Meon` or compile to native executable. It is distributed under [MIT License](LICENSE).

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

Here are some FAQ about `Meon` project.

-   WHY IT'S NAMED `Meon` ?

    -   it's from **Melon** and then, I removed **L**.
    -   It's like `Neon`, `Xenon` and sounds sweet.

-   WHY THE FU\*K I CREATED THIS ?
    -   No reason! I just want to learn something new by creating something new.
    -   I gain massive knowledge about **How CPU works**, **How machine interpret and execute various instructions and jump to and from**.

## License

The `Meon` Interpreter is licensed under MIT. See [LICENSE](LICENSE) for more details.
