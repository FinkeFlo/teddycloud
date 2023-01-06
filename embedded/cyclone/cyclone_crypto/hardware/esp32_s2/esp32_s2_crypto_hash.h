/**
 * @file esp32_s2_crypto_hash.h
 * @brief ESP32-S2 hash hardware accelerator
 *
 * @section License
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2010-2022 Oryx Embedded SARL. All rights reserved.
 *
 * This file is part of CycloneCRYPTO Open.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @author Oryx Embedded SARL (www.oryx-embedded.com)
 * @version 2.2.0
 **/

#ifndef _ESP32_S2_CRYPTO_HASH_H
#define _ESP32_S2_CRYPTO_HASH_H

//Dependencies
#include "core/crypto.h"

//Hash hardware accelerator
#ifndef ESP32_S2_CRYPTO_HASH_SUPPORT
   #define ESP32_S2_CRYPTO_HASH_SUPPORT DISABLED
#elif (ESP32_S2_CRYPTO_HASH_SUPPORT != ENABLED && ESP32_S2_CRYPTO_HASH_SUPPORT != DISABLED)
   #error ESP32_S2_CRYPTO_HASH_SUPPORT parameter is not valid
#endif

//C++ guard
#ifdef __cplusplus
extern "C" {
#endif

//Hash related functions
void esp32s2ShaInit(void);

//C++ guard
#ifdef __cplusplus
}
#endif

#endif
