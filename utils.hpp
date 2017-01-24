#ifndef _UTILS_HPP_
#define _UTILS_HPP_

#include <iostream>
#include <cmath>
#include <stdexcept>

#include <soundio/soundio.h>

template <typename T, typename U = T> 
typename std::common_type<T, U>::type positive_modulo(const T& dividend, 
                                                      const U& divisor) {  
  return ((dividend % divisor) + divisor) % divisor;  
}

inline double positive_modulof(double a, double b) {
  return std::fmod(std::fmod(a, b) + b, b);
}

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
void call_sio(F f, Args&&... args) {
    int error = f(std::forward<Args>(args)...);
    if (error != 0) {
        const char* text = soundio_strerror(error);
        throw std::runtime_error(
          "Soundio error " + std::to_string(error) + " " + text);
    }
}

#endif