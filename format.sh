#!/bin/bash

###
 # Copyright 2022-2023 SPACEMIT. All rights reserved.
 # Use of this source code is governed by a BSD-style license
 # that can be found in the LICENSE file.
 # 
 # @Author: David(qiang.fu@spacemit.com)
 # @Date: 2023-01-18 11:26:26
 # @LastEditTime: 2023-11-02 17:56:41
 # @Description: code style optimization, use clang-format to format the code
### 

###
 # How to use
 # 1.  apt install clang-format(if not root, do it in docker)
 # 2.  su username, switch from root to normal
 # 3.  ./format.sh in the mpp root directory
 # 4.  check the code and verify it again, code-stype optimization may cause trouble
###

find . -name "*.cc" -o -name "*.c" -o -name "*.h" -o -name "*.hh" -o -name "*.cpp" -o -name "*.hpp" | xargs clang-format -style=Google -i
git checkout include/mvx-v4l2-controls.h
git checkout include/fw_v2/mve_protocol_def.h
git checkout include/mvx_log_ram.h
