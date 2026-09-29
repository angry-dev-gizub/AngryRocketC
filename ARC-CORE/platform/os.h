/*
 * Copyright 2026 Nelson Somé
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for license information.
 */

/**
 * @file os.h
 *
 * @author Nelson Somé
 *
 * @brief OS detection.
 *
 * This file is used to detect the operating system on which Arc is compiled.
 *
 * @note This file is part of Arc Core.
 */

#ifndef ARC_OS_H_
#define ARC_OS_H_

#define ARC_PLATFORM_WINDOWS 0
#define ARC_PLATFORM_LINUX   0
#define ARC_PLATFORM_MACOS   0

#if defined(_WIN32)
#undef ARC_PLATFORM_WINDOWS
#define ARC_PLATFORM_WINDOWS 1
#elif defined(__linux__)
#undef ARC_PLATFORM_LINUX
#define ARC_PLATFORM_LINUX 1
#elif defined(__APPLE__)
#undef ARC_PLATFORM_MACOS
#define ARC_PLATFORM_MACOS 1
#else
#error "Arc: Operating system not supported."
#endif

#endif /* ARC_OS_H_ */