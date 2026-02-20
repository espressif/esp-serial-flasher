/* Copyright 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#error "serial_io.h has been removed in v2. The free-function port API \
(loader_port_write, loader_port_read, loader_port_change_transmission_rate, …) \
no longer exists. Port implementations must now provide an esp_loader_ *_port_ops_t \
vtable and include esp_loader_io.h instead. \
See docs / migration - v1 - to - v2.md Section 4 for upgrade instructions."
