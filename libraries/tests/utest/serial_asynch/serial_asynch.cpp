/* mbed Microcontroller Library
 * Copyright (c) 2015 ARM Limited
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
#include <stdio.h>
#include "TestHarness.h"
#include "mbed.h"

/* Serial asynch cross */

#if !DEVICE_SERIAL || !DEVICE_SERIAL_ASYNCH
#error serial_asynch requires asynch Serial
#endif

// Device config
#if defined(TARGET_K64F)
#define TEST_SERIAL_ONE_TX_PIN PTC17 // uart3
#define TEST_SERIAL_TWO_RX_PIN PTD2  // uart2

#else

#error Target not supported
#endif

// Test config
#define SHORT_XFR 3
#define LONG_XFR 16
#define TEST_BYTE_TX_BASE 0x5555
#define TEST_BYTE_RX      0x5A5A

volatile int tx_event_flag;
volatile bool tx_complete;

volatile int rx_event_flag;
volatile bool rx_complete;

void cb_tx_done(int event)
{
    tx_complete = true;
    tx_event_flag = event;
}

void cb_rx_done(int event)
{
    rx_complete = true;
    rx_event_flag = event;
}

TEST_GROUP(Serial_Asynchronous)
{
    uint8_t tx_buf[LONG_XFR];
    uint8_t rx_buf[LONG_XFR];

    Serial *serial_tx;
    Serial *serial_rx;
    event_callback_t tx_callback;
    event_callback_t rx_callback;

    void setup()
    {
        serial_tx = new Serial(TEST_SERIAL_ONE_TX_PIN, NC);
        serial_rx = new Serial(NC, TEST_SERIAL_TWO_RX_PIN);
        tx_complete = false;
        tx_event_flag = 0;
        rx_complete = false;
        rx_event_flag = 0;
        tx_callback.attach(cb_tx_done);
        rx_callback.attach(cb_rx_done);

        // Set the default value of tx_buf
        for (uint32_t i = 0; i < sizeof(tx_buf); i++) {
            tx_buf[i] = i + TEST_BYTE_TX_BASE;
        }
        memset(rx_buf, TEST_BYTE_RX, sizeof(rx_buf));
    }

    void teardown()
    {
        delete serial_tx;
        serial_tx = NULL;
        delete serial_rx;
        serial_rx = NULL;

    }

    uint32_t cmpnbufc(uint8_t expect, uint8_t *actual, uint32_t offset, uint32_t end, const char *file, uint32_t line)
    {
        uint32_t i;
        for (i = offset; i < end; i++){
            if (expect != actual[i]) {
                break;
            }
        }
        if (i < end) {
            CHECK_EQUAL_LOCATION((int)expect, (int)actual[i], file, line);
        }
        CHECK_EQUAL_LOCATION(end, i, file, line);
        return i;
    }

    uint32_t cmpnbuf(uint8_t *expect, uint8_t *actual, uint32_t offset, uint32_t end, const char *file, uint32_t line)
    {
        uint32_t i;
        for (i = offset; i < end; i++){
            if (expect[i] != actual[i]) {
                break;
            }
        }
        if (i < end) {
            CHECK_EQUAL_LOCATION((int)expect[i], (int)actual[i], file, line);
        }
        CHECK_EQUAL_LOCATION(end, i, file, line);
        return i;
    }

};

TEST(Serial_Asynchronous, short_tx_0_rx)
{
    int rc;
    rc = serial_tx->write(tx_buf, SHORT_XFR, tx_callback, -1);
    CHECK_EQUAL(0, rc);

    while (!tx_complete);

    CHECK_EQUAL(SERIAL_EVENT_TX_COMPLETE, tx_event_flag);
    // rx buffer unchanged
    cmpnbufc(TEST_BYTE_RX, rx_buf, 0, sizeof(rx_buf), __FILE__, __LINE__);
}

TEST(Serial_Asynchronous, short_tx_short_rx)
{
    int rc;
    serial_rx->read(rx_buf, SHORT_XFR, rx_callback, -1);
    rc = serial_tx->write(tx_buf, SHORT_XFR, tx_callback, -1);
    CHECK_EQUAL(0, rc);

    while ((!tx_complete) || (!rx_complete));

    CHECK_EQUAL(SERIAL_EVENT_TX_COMPLETE, tx_event_flag);
    CHECK_EQUAL(SERIAL_EVENT_RX_COMPLETE, rx_event_flag);

    // Check that the receive buffer contains the fill byte.
    cmpnbuf(tx_buf, rx_buf, 0, SHORT_XFR, __FILE__, __LINE__);
    // Check that remaining portion of the receive buffer contains the rx test byte
    cmpnbufc(TEST_BYTE_RX, rx_buf, SHORT_XFR, sizeof(rx_buf), __FILE__, __LINE__);
}

TEST(Serial_Asynchronous, long_tx_long_rx)
{
    int rc;
    serial_rx->read(rx_buf, LONG_XFR, rx_callback, -1);
    rc = serial_tx->write(tx_buf, LONG_XFR, tx_callback, -1);
    CHECK_EQUAL(0, rc);

    while ((!tx_complete) || (!rx_complete));

    CHECK_EQUAL(SERIAL_EVENT_TX_COMPLETE, tx_event_flag);
    CHECK_EQUAL(SERIAL_EVENT_RX_COMPLETE, rx_event_flag);

    // Check that the receive buffer contains the fill byte.
    cmpnbuf(tx_buf, rx_buf, 0, LONG_XFR, __FILE__, __LINE__);
    // Check that remaining portion of the receive buffer contains the rx test byte
    cmpnbufc(TEST_BYTE_RX, rx_buf, LONG_XFR, sizeof(rx_buf), __FILE__, __LINE__);
}

TEST(Serial_Asynchronous, rx_parity_error)
{
    int rc;
    // Set different parity for RX and TX
    serial_rx->format(8, SerialBase::Even, 1);
    serial_tx->format(8, SerialBase::Odd, 1);
    serial_rx->read(rx_buf, LONG_XFR, rx_callback, -1);
    rc = serial_tx->write(tx_buf, LONG_XFR, tx_callback, -1);
    CHECK_EQUAL(0, rc);

    while ((!tx_complete) || (!rx_complete));

    CHECK_EQUAL(SERIAL_EVENT_TX_COMPLETE, tx_event_flag);
    CHECK_EQUAL(SERIAL_EVENT_RX_PARITY_ERROR, rx_event_flag);
}

TEST(Serial_Asynchronous, rx_framing_error)
{
   int rc;
   serial_tx->baud(4800);
   serial_rx->read(rx_buf, LONG_XFR, rx_callback, -1);
   rc = serial_tx->write(tx_buf, LONG_XFR, tx_callback, -1);
   CHECK_EQUAL(0, rc);

   while ((!tx_complete) || (!rx_complete));

   CHECK_EQUAL(SERIAL_EVENT_TX_COMPLETE, tx_event_flag);
   CHECK_EQUAL(SERIAL_EVENT_RX_FRAMING_ERROR, rx_event_flag);
}

TEST(Serial_Asynchronous, char_matching_success)
{
    // match found
    serial_rx->read(rx_buf, LONG_XFR, rx_callback, -1, (uint8_t)(TEST_BYTE_TX_BASE+5));
    serial_tx->write(tx_buf, LONG_XFR, tx_callback, -1);

    while ((!tx_complete) || (!rx_complete));

    CHECK_EQUAL(SERIAL_EVENT_TX_COMPLETE, tx_event_flag);
    CHECK_EQUAL(SERIAL_EVENT_RX_CHARACTER_MATCH, rx_event_flag);

    cmpnbufc(TEST_BYTE_RX, rx_buf, 5, sizeof(rx_buf), __FILE__, __LINE__);
}

TEST(Serial_Asynchronous, char_matching_failed)
{
    // no match found (specified match char is not in tx buffer)
    serial_rx->read(rx_buf, LONG_XFR, rx_callback, -1, (uint8_t)(TEST_BYTE_TX_BASE  + sizeof(tx_buf)));
    serial_tx->write(tx_buf, LONG_XFR, tx_callback, -1);

    while ((!tx_complete) || (!rx_complete));

    CHECK_EQUAL(SERIAL_EVENT_TX_COMPLETE, tx_event_flag);
    CHECK_EQUAL(SERIAL_EVENT_RX_COMPLETE, rx_event_flag);

    cmpnbuf(tx_buf, rx_buf, 0, LONG_XFR, __FILE__, __LINE__);
}

TEST(Serial_Asynchronous, char_matching_with_complete)
{
    serial_rx->read(rx_buf, LONG_XFR, rx_callback, -1, (uint8_t)(TEST_BYTE_TX_BASE  + sizeof(tx_buf) - 1));
    serial_tx->write(tx_buf, LONG_XFR, tx_callback, -1);

    while ((!tx_complete) || (!rx_complete));

    CHECK_EQUAL(SERIAL_EVENT_TX_COMPLETE, tx_event_flag);
    CHECK_EQUAL((SERIAL_EVENT_RX_COMPLETE | SERIAL_EVENT_RX_CHARACTER_MATCH), rx_event_flag);

    cmpnbuf(tx_buf, rx_buf, 0, LONG_XFR, __FILE__, __LINE__);
}
