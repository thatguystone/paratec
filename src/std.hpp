/**
 * @file
 * @author Andrew Stone <a@stoney.io>
 * @copyright 2015 Andrew Stone
 *
 * This file is part of paratec and is released under the MIT License:
 * http://opensource.org/licenses/MIT
 */

#pragma once

#include <memory>

#define INDENT "    "
#define NELS(a) (sizeof(a) / sizeof((a)[0]))

template <typename T> using sp = std::shared_ptr<T>;

template <typename T, typename... Args>
inline std::shared_ptr<T> mksp(Args &&... args)
{
	return std::make_shared<T>(std::forward<Args>(args)...);
}
