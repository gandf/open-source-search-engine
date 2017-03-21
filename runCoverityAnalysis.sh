#!/bin/bash
# Original from https://scan.coverity.com/scripts/travisci_build_coverity_scan.sh

# Exit on failure
set -e

if [ "${TRAVIS_JOB_NUMBER##*.}" != "1" ] || \
   [ "${TRAVIS_PULL_REQUEST}" = "true" ]
then
  echo "Skipping coverity scan."
  exit
fi

# setup parameters
export COVERITY_SCAN_PROJECT_NAME="os search engine"
export COVERITY_SCAN_NOTIFICATION_EMAIL="alc@privacore.com"
export COVERITY_SCAN_BUILD_COMMAND_PREPEND="make clean && make libcld2_full.so slacktee.sh"
export COVERITY_SCAN_BUILD_COMMAND="make"
export COVERITY_SCAN_BRANCH_PATTERN="master"

PLATFORM=`uname`
TOOL_ARCHIVE=/tmp/cov-analysis-${PLATFORM}.tgz
TOOL_URL=https://scan.coverity.com/download/${PLATFORM}
TOOL_BASE=/tmp/coverity-scan-analysis
UPLOAD_URL="https://scan.coverity.com/builds?project=os+search+engine"
SCAN_URL="https://scan.coverity.com"

# Do not run on pull requests
if [ "${TRAVIS_PULL_REQUEST}" = "true" ]; then
  echo -e "\033[33;1mINFO: Skipping Coverity Analysis: branch is a pull request.\033[0m"
  exit 0
fi

# Verify this branch should run
IS_COVERITY_SCAN_BRANCH=`ruby -e "puts '${TRAVIS_BRANCH}' =~ /\\A$COVERITY_SCAN_BRANCH_PATTERN\\z/ ? 1 : 0"`
if [ "$IS_COVERITY_SCAN_BRANCH" = "1" ]; then
  echo -e "\033[33;1mCoverity Scan configured to run on branch ${TRAVIS_BRANCH}\033[0m"
else
  echo -e "\033[33;1mCoverity Scan NOT configured to run on branch ${TRAVIS_BRANCH}\033[0m"
  exit 1
fi

# Verify upload is permitted
AUTH_RES=`curl -s --form project="$COVERITY_SCAN_PROJECT_NAME" --form token="$COVERITY_SCAN_TOKEN" $SCAN_URL/api/upload_permitted`
if [ "$AUTH_RES" = "Access denied" ]; then
  echo -e "\033[33;1mCoverity Scan API access denied. Check COVERITY_SCAN_PROJECT_NAME and COVERITY_SCAN_TOKEN.\033[0m"
  exit 1
else
  AUTH=`echo $AUTH_RES | ruby -e "require 'rubygems'; require 'json'; puts JSON[STDIN.read]['upload_permitted']"`
  if [ "$AUTH" = "true" ]; then
    echo -e "\033[33;1mCoverity Scan analysis authorized per quota.\033[0m"
  else
    WHEN=`echo $AUTH_RES | ruby -e "require 'rubygems'; require 'json'; puts JSON[STDIN.read]['next_upload_permitted_at']"`
    echo -e "\033[33;1mCoverity Scan analysis NOT authorized until $WHEN.\033[0m"
    exit 0
  fi
fi

if [ ! -d $TOOL_BASE ]; then
  # Download Coverity Scan Analysis Tool
  if [ ! -e $TOOL_ARCHIVE ]; then
    echo -e "\033[33;1mDownloading Coverity Scan Analysis Tool...\033[0m"
    wget -nv -O $TOOL_ARCHIVE $TOOL_URL --post-data "project=$COVERITY_SCAN_PROJECT_NAME&token=$COVERITY_SCAN_TOKEN"
  fi

  # Extract Coverity Scan Analysis Tool
  echo -e "\033[33;1mExtracting Coverity Scan Analysis Tool...\033[0m"
  mkdir -p $TOOL_BASE
  pushd $TOOL_BASE
  tar xzf $TOOL_ARCHIVE
  popd
fi

TOOL_DIR=`find $TOOL_BASE -type d -name 'cov-analysis*'`
export PATH=$TOOL_DIR/bin:$PATH

# Configure
echo -e "\033[33;1mRunning Coverity Scan Configure Tool...\033[0m"
cov-configure -co g++-5 -- -std=c++11 -march=core-avx-i -msse4.2

# Build
echo -e "\033[33;1mRunning Coverity Scan Analysis Tool...\033[0m"
COV_BUILD_OPTIONS=""
#COV_BUILD_OPTIONS="--return-emit-failures 8 --parse-error-threshold 85"
RESULTS_DIR="cov-int"
eval "${COVERITY_SCAN_BUILD_COMMAND_PREPEND}"
COVERITY_UNSUPPORTED=1 cov-build --dir $RESULTS_DIR $COV_BUILD_OPTIONS $COVERITY_SCAN_BUILD_COMMAND
cov-import-scm --dir $RESULTS_DIR --scm git --log $RESULTS_DIR/scm_log.txt 2>&1

# show log
echo -e "\033[33;1mShow Coverity Scan build-log...\033[0m"
cat $RESULTS_DIR/build-log.txt

echo -e "\033[33;1mShow Coverity Scan scm_log...\033[0m"
cat $RESULTS_DIR/scm_log.txt

# Upload results
echo -e "\033[33;1mTarring Coverity Scan Analysis results...\033[0m"
RESULTS_ARCHIVE=analysis-results.tgz
tar czf $RESULTS_ARCHIVE $RESULTS_DIR
SHA=`git rev-parse --short HEAD`

echo -e "\033[33;1mUploading Coverity Scan Analysis results...\033[0m"
response=$(curl \
  --silent --write-out "\n%{http_code}\n" \
  --form token=$COVERITY_SCAN_TOKEN \
  --form email=$COVERITY_SCAN_NOTIFICATION_EMAIL \
  --form file=@$RESULTS_ARCHIVE \
  --form version=$SHA \
  --form description="Travis CI build" \
  $UPLOAD_URL)

status_code=$(echo "$response" | sed -n '$p')
if [ "$status_code" != "201" ]; then
  TEXT=$(echo "$response" | sed '$d')
  echo -e "\033[33;1mCoverity Scan upload failed: $TEXT.\033[0m"
  exit 1
fi
