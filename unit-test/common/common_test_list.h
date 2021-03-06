/*
 * Copyright 2017 Nest Labs, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __DETECTORGRAPH_UNIT_TEST_COMMON_TEST_LIST_H__
#define __DETECTORGRAPH_UNIT_TEST_COMMON_TEST_LIST_H__

/* (1) INCLUDE YOUR TEST HERE */

#include "test_foodetector.h"
#include "test_futurepublisher.h"
#include "test_graphinputqueue.h"
#include "test_lag.h"
#include "test_subscriptiondispatcherscontainer.h"
#include "test_timeoutpublisher.h"
#include "test_timeoutpublisherservice.h"
#include "test_testtimeoutpublisherservice.h"
#include "test_topic.h"
#include "test_topicregistry.h"

#define COMMON_TEST_LIST \
    foodetector_testsuite, \
    futurepublisher_testsuite, \
    graphinputqueue_testsuite, \
    lag_testsuite, \
    subscriptiondispatcherscontainer_testsuite, \
    timeoutpublisher_testsuite, \
    timeoutpublisherservice_testsuite, \
    testtimeoutpublisherservice_testsuite, \
    topic_testsuite, \
    topicregistry_testsuite, \

#endif // __DETECTORGRAPH_UNIT_TEST_COMMON_TEST_LIST_H__
