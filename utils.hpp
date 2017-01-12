#ifndef _UTILS_HPP_
#define _UTILS_HPP_

#include <iostream>
#include <stdexcept>

#include <portaudio.h>

template <typename O>
void _dbg(O& out) {
  out << std::endl;
}

template <typename O, typename Arg>
void _dbg(O& out, Arg arg) {
  out << arg;
  _dbg(out);
}

template <typename O, typename Arg, typename Arg2, typename... Args>
void _dbg(O& out, Arg first, Arg2 second, Args... args) {
  out << first << ", ";
  _dbg(out, second, args...);
}

template <typename... Args>
void print(Args... args) {
  _dbg(std::cout, args...);
}

#ifdef DEBUG
template <typename... Args>
void dbg(Args... args) {
  _dbg(std::cerr, args...);
}
#else
template <typename... Args>
void dbg(Args... args) {}
#endif

template<typename F, typename... Args>
void call_pa(F f, Args&&... args) {
    PaError error = f(std::forward<Args>(args)...);
    if (error != paNoError) {
        throw std::runtime_error("PaError " + std::to_string(error));
    }
}

#endif