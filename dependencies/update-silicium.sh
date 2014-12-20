#!/usr/bin/env bash
set -e
cd silicium
git fetch
git merge origin/master --ff-only
