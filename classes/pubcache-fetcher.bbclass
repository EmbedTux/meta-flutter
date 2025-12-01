# Copyright (C) 2025 Jonathan Poulin (EmbedTux)
#
# SPDX-License-Identifier: MIT
#

python () {
    import pubcache
    bb.fetch2.methods.append(pubcache.PUB_CACHE())
}

