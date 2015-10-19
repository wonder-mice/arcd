# arcd

Arithmetic coding is a form of entropy encoding used in lossless data
compression. This library provides simple, easy to understand implementation of
arithmetic coding. It produces same output you expect from more advanced
libraries, however it doesn't have fancy performance optimizations.
Goals:
* Portable (no external dependencies, C language)
* Easy to use (minimal straightforward API)
* Simple to experiment with (clean, compact implementation)

Keep in mind, that for arithmetic coding to be efficient you need to build a
decent probability model of your data. If model is accurate you can get very
close to theoretical compression limit. This library doesn't provide any default
models.
