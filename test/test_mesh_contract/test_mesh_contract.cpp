// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// This file is part of SigurdOS.
//
// SigurdOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SigurdOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SigurdOS.  If not, see <https://www.gnu.org/licenses/>.

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

#include "mesh/mesh_wrapper.h"
#include "mesh/public_channel.h"

namespace {

using sigurdos::mesh::ContactInfo;
using sigurdos::mesh::GroupDataType;
using sigurdos::mesh::MeshMessage;
using sigurdos::mesh::NodeStatus;
using sigurdos::mesh::PacketLogEntry;
using sigurdos::mesh::TelemetryItem;
using sigurdos::mesh::TelemetryResult;

TEST(MeshContractTest, AdvertTypesMatchMeshCoreCompanionValues) {
    EXPECT_EQ(ADV_TYPE_NONE, 0);
    EXPECT_EQ(ADV_TYPE_CHAT, 1);
    EXPECT_EQ(ADV_TYPE_REPEATER, 2);
    EXPECT_EQ(ADV_TYPE_ROOM, 3);
    EXPECT_EQ(ADV_TYPE_SENSOR, 4);
}

TEST(MeshContractTest, AutoAddConfigAllowsConfiguredAdvertTypes) {
    EXPECT_TRUE(sigurdos::mesh::autoAddConfigAllowsContactType(ADV_TYPE_CHAT, 0x1E));
    EXPECT_TRUE(sigurdos::mesh::autoAddConfigAllowsContactType(ADV_TYPE_REPEATER, 0x1E));
    EXPECT_TRUE(sigurdos::mesh::autoAddConfigAllowsContactType(ADV_TYPE_ROOM, 0x1E));
    EXPECT_TRUE(sigurdos::mesh::autoAddConfigAllowsContactType(ADV_TYPE_SENSOR, 0x1E));
    EXPECT_FALSE(sigurdos::mesh::autoAddConfigAllowsContactType(ADV_TYPE_NONE, 0x1E));
}

TEST(MeshContractTest, AutoAddConfigCanDisableOnlyRepeaters) {
    uint8_t no_repeaters = (1u << ADV_TYPE_CHAT)
                         | (1u << ADV_TYPE_ROOM)
                         | (1u << ADV_TYPE_SENSOR);
    EXPECT_TRUE(sigurdos::mesh::autoAddConfigAllowsContactType(ADV_TYPE_CHAT, no_repeaters));
    EXPECT_FALSE(sigurdos::mesh::autoAddConfigAllowsContactType(ADV_TYPE_REPEATER, no_repeaters));
}

TEST(MeshContractTest, AutoAddOverwriteOldestUsesBitZero) {
    EXPECT_FALSE(sigurdos::mesh::autoAddConfigAllowsOverwriteOldest(0x1E));
    EXPECT_TRUE(sigurdos::mesh::autoAddConfigAllowsOverwriteOldest(0x1F));
    EXPECT_TRUE(sigurdos::mesh::autoAddConfigAllowsOverwriteOldest(0x01));
}

TEST(MeshContractTest, PublicContactIndexesSkipMeshCoreAnonSlots) {
    EXPECT_EQ(sigurdos::mesh::detail::CONTACT_PUBLIC_INDEX_OFFSET, 8u);
    EXPECT_EQ(sigurdos::mesh::detail::contactPublicIndexToRawSlot(0), 8u);
    EXPECT_EQ(sigurdos::mesh::detail::contactPublicIndexToRawSlot(12), 20u);
}

TEST(MeshContractTest, PublicChannelDefaultsStayStable) {
    EXPECT_STREQ(sigurdos::mesh::PUBLIC_CHANNEL_NAME, "Public");
    EXPECT_STREQ(sigurdos::mesh::PUBLIC_CHANNEL_PSK_BASE64,
                 "izOH6cXN6mrJ5e26oRXNcg==");
    EXPECT_TRUE(sigurdos::mesh::isPublicChannelName("Public"));
    EXPECT_FALSE(sigurdos::mesh::isPublicChannelName("#public"));
    EXPECT_FALSE(sigurdos::mesh::isPublicChannelName(nullptr));
    EXPECT_TRUE(sigurdos::mesh::isReservedPublicHashtagName("Public"));
    EXPECT_TRUE(sigurdos::mesh::isReservedPublicHashtagName("#Public"));
    EXPECT_TRUE(sigurdos::mesh::isReservedPublicHashtagName(" #public "));
    EXPECT_FALSE(sigurdos::mesh::isReservedPublicHashtagName("#public-room"));
    EXPECT_FALSE(sigurdos::mesh::isReservedPublicHashtagName(nullptr));
}

TEST(MeshContractTest, ContactPermissionsFitPackedFlagBits) {
    EXPECT_EQ(PERM_ACL_GUEST, 0);
    EXPECT_EQ(PERM_ACL_READ_ONLY, 1);
    EXPECT_EQ(PERM_ACL_READ_WRITE, 2);
    EXPECT_EQ(PERM_ACL_ADMIN, 3);
}

TEST(MeshContractTest, LoginStatusValuesStayStableForUiStateMachine) {
    EXPECT_EQ(LOGIN_STATUS_NONE, 0);
    EXPECT_EQ(LOGIN_STATUS_PENDING, 1);
    EXPECT_EQ(LOGIN_STATUS_OK, 2);
    EXPECT_EQ(LOGIN_STATUS_FAILED, 3);
}

TEST(MeshContractTest, LoginStatusCancelPolicyMatchesPendingUiStates) {
    EXPECT_FALSE(sigurdos::mesh::loginStatusNeedsLocalCancel(LOGIN_STATUS_NONE));
    EXPECT_TRUE(sigurdos::mesh::loginStatusNeedsLocalCancel(LOGIN_STATUS_PENDING));
    EXPECT_FALSE(sigurdos::mesh::loginStatusNeedsLocalCancel(LOGIN_STATUS_OK));
    EXPECT_TRUE(sigurdos::mesh::loginStatusNeedsLocalCancel(LOGIN_STATUS_FAILED));

    EXPECT_TRUE(sigurdos::mesh::loginStatusCanBeReclaimed(LOGIN_STATUS_NONE));
    EXPECT_FALSE(sigurdos::mesh::loginStatusCanBeReclaimed(LOGIN_STATUS_PENDING));
    EXPECT_FALSE(sigurdos::mesh::loginStatusCanBeReclaimed(LOGIN_STATUS_OK));
    EXPECT_TRUE(sigurdos::mesh::loginStatusCanBeReclaimed(LOGIN_STATUS_FAILED));
}

TEST(MeshContractTest, LoginPendingTimeoutUsesWrapSafeElapsedTime) {
    EXPECT_EQ(sigurdos::mesh::LOGIN_PENDING_TIMEOUT_MS, 30000u);
    EXPECT_FALSE(sigurdos::mesh::loginPendingTimedOut(29999u, 0u));
    EXPECT_TRUE(sigurdos::mesh::loginPendingTimedOut(30000u, 0u));
    EXPECT_TRUE(sigurdos::mesh::loginPendingTimedOut(100u, UINT32_MAX - 29900u));
}

TEST(MeshContractTest, LoginKeepAliveUsesDefaultForMeshCoreZeroHint) {
    EXPECT_EQ(sigurdos::mesh::LOGIN_DEFAULT_KEEP_ALIVE_SECS, 60u);
    EXPECT_EQ(sigurdos::mesh::loginKeepAliveSeconds(0, ADV_TYPE_REPEATER), 60u);
    EXPECT_EQ(sigurdos::mesh::loginKeepAliveSeconds(0, ADV_TYPE_ROOM), 60u);
    EXPECT_EQ(sigurdos::mesh::loginKeepAliveSeconds(4, ADV_TYPE_ROOM), 64u);
    EXPECT_EQ(sigurdos::mesh::loginKeepAliveSeconds(0, ADV_TYPE_CHAT), 0u);
}

TEST(MeshContractTest, LoginResponseParserAcceptsNewOkWithExtendedPayload) {
    const uint8_t ok[] = {
        0x11, 0x22, 0x33, 0x44,
        0x00, 0x04, 0x02, 0x7f,
        0x00, 0x00, 0x00, 0x00,
        0x12
    };
    auto parsed = sigurdos::mesh::parseLoginResponse(ok, sizeof(ok), true);
    EXPECT_EQ(parsed.kind, sigurdos::mesh::LoginResponseKind::NewOk);
    EXPECT_EQ(parsed.keep_alive_units, 0x04);
    EXPECT_EQ(parsed.permission, 0x02);
    EXPECT_EQ(parsed.acl, 0x7f);
    EXPECT_EQ(parsed.server_tag, 0x44332211u);
    EXPECT_EQ(parsed.firmware_level, 0x12);

}

TEST(MeshContractTest, LoginResponseParserAcceptsShortNewOkWithSafeDefaults) {
    const uint8_t short_ok[] = {0x11, 0x22, 0x33, 0x44, 0x00};
    auto parsed = sigurdos::mesh::parseLoginResponse(short_ok, sizeof(short_ok), true);
    EXPECT_EQ(parsed.kind, sigurdos::mesh::LoginResponseKind::NewOk);
    EXPECT_EQ(parsed.server_tag, 0x44332211u);
    EXPECT_EQ(parsed.keep_alive_units, 0u);
    EXPECT_EQ(parsed.permission, 0u);
    EXPECT_EQ(parsed.acl, 0u);
    EXPECT_EQ(parsed.firmware_level, 0u);
}

TEST(MeshContractTest, LoginResponseParserAcceptsLegacyOk) {
    const uint8_t legacy_ok[] = {0x11, 0x22, 0x33, 0x44, 'O', 'K'};
    auto parsed = sigurdos::mesh::parseLoginResponse(legacy_ok, sizeof(legacy_ok), true);
    EXPECT_EQ(parsed.kind, sigurdos::mesh::LoginResponseKind::LegacyOk);
    EXPECT_EQ(parsed.permission, 1);
}

TEST(MeshContractTest, EffectiveLoginPermissionDoesNotTreatRoomReadOnlyHintAsAdmin) {
    EXPECT_EQ(sigurdos::mesh::effectiveLoginPermission(2, 0), PERM_ACL_GUEST);
    EXPECT_EQ(sigurdos::mesh::effectiveLoginPermission(1, 0), PERM_ACL_ADMIN);
    EXPECT_EQ(sigurdos::mesh::effectiveLoginPermission(0, PERM_ACL_READ_ONLY),
              PERM_ACL_READ_ONLY);
    EXPECT_EQ(sigurdos::mesh::effectiveLoginPermission(1, PERM_ACL_READ_WRITE),
              PERM_ACL_READ_WRITE);
    EXPECT_EQ(sigurdos::mesh::effectiveLoginPermission(1, PERM_ACL_ADMIN),
              PERM_ACL_ADMIN);
}

TEST(MeshContractTest, LoginResponseParserMarksPendingFailures) {
    const uint8_t failed[] = {0x11, 0x22, 0x33, 0x44, 0x05};
    auto parsed = sigurdos::mesh::parseLoginResponse(failed, sizeof(failed), true);
    EXPECT_EQ(parsed.kind, sigurdos::mesh::LoginResponseKind::Failed);
    EXPECT_EQ(parsed.failure_code, 0x05);

    parsed = sigurdos::mesh::parseLoginResponse(failed, sizeof(failed), false);
    EXPECT_EQ(parsed.kind, sigurdos::mesh::LoginResponseKind::Ignored);
}

TEST(MeshContractTest, PendingRequestsExpireWithWrapSafeElapsedTime) {
    EXPECT_EQ(sigurdos::mesh::PENDING_REQUEST_TTL_MS, 120000u);
    EXPECT_FALSE(sigurdos::mesh::pendingRequestExpired(0u, 119999u));
    EXPECT_TRUE(sigurdos::mesh::pendingRequestExpired(0u, 120000u));
    EXPECT_TRUE(sigurdos::mesh::pendingRequestExpired(UINT32_MAX - 100u, 119899u));
}

TEST(MeshContractTest, LoginPasswordPolicyMatchesMeshCoreRoomLogin) {
    EXPECT_EQ(sigurdos::mesh::LOGIN_PASSWORD_MAX_BYTES, 15u);
    EXPECT_TRUE(sigurdos::mesh::loginPasswordInputSubmittable(""));
    EXPECT_TRUE(sigurdos::mesh::loginPasswordInputSubmittable("secret"));
    EXPECT_TRUE(sigurdos::mesh::loginPasswordInputSubmittable("123456789012345"));
    EXPECT_FALSE(sigurdos::mesh::loginPasswordInputSubmittable("1234567890123456"));
    EXPECT_FALSE(sigurdos::mesh::loginPasswordInputSubmittable(nullptr));

    EXPECT_TRUE(sigurdos::mesh::loginPasswordAllowedForContactType(ADV_TYPE_ROOM, ""));
    EXPECT_TRUE(sigurdos::mesh::loginPasswordAllowedForContactType(ADV_TYPE_ROOM, "admin"));
    EXPECT_TRUE(sigurdos::mesh::loginPasswordAllowedForContactType(ADV_TYPE_REPEATER, ""));
    EXPECT_TRUE(sigurdos::mesh::loginPasswordAllowedForContactType(ADV_TYPE_REPEATER, "guest"));
    EXPECT_FALSE(sigurdos::mesh::loginPasswordAllowedForContactType(ADV_TYPE_CHAT, ""));
    EXPECT_FALSE(sigurdos::mesh::loginPasswordAllowedForContactType(ADV_TYPE_SENSOR, "guest"));
    EXPECT_FALSE(sigurdos::mesh::loginPasswordAllowedForContactType(ADV_TYPE_NONE, ""));

    EXPECT_TRUE(sigurdos::mesh::loginContactTypeMatchesHint(ADV_TYPE_REPEATER, ADV_TYPE_NONE));
    EXPECT_TRUE(sigurdos::mesh::loginContactTypeMatchesHint(ADV_TYPE_ROOM, ADV_TYPE_NONE));
    EXPECT_FALSE(sigurdos::mesh::loginContactTypeMatchesHint(ADV_TYPE_CHAT, ADV_TYPE_NONE));
    EXPECT_TRUE(sigurdos::mesh::loginContactTypeMatchesHint(ADV_TYPE_ROOM, ADV_TYPE_ROOM));
    EXPECT_FALSE(sigurdos::mesh::loginContactTypeMatchesHint(ADV_TYPE_REPEATER, ADV_TYPE_ROOM));

    EXPECT_TRUE(sigurdos::mesh::loginShouldForceFloodForContactType(ADV_TYPE_ROOM));
    EXPECT_TRUE(sigurdos::mesh::loginShouldForceFloodForContactType(ADV_TYPE_REPEATER));
    EXPECT_FALSE(sigurdos::mesh::loginShouldForceFloodForContactType(ADV_TYPE_CHAT));

    EXPECT_TRUE(sigurdos::mesh::loginBootstrapShouldBypassFloodScope(ADV_TYPE_ROOM));
    EXPECT_TRUE(sigurdos::mesh::loginBootstrapShouldBypassFloodScope(ADV_TYPE_REPEATER));
    EXPECT_FALSE(sigurdos::mesh::loginBootstrapShouldBypassFloodScope(ADV_TYPE_CHAT));
    EXPECT_FALSE(sigurdos::mesh::loginBootstrapShouldBypassFloodScope(ADV_TYPE_SENSOR));
}

TEST(MeshContractTest, NeighboursRequestMatchesMeshCoreSimpleRepeaterFormat) {
    uint8_t req[11] = {0};
    ASSERT_TRUE(sigurdos::mesh::buildNeighboursRequest(
        req, sizeof(req),
        sigurdos::mesh::NEIGHBOUR_DEFAULT_COUNT,
        0x1234,
        sigurdos::mesh::NEIGHBOUR_DEFAULT_ORDER_NEWEST,
        sigurdos::mesh::NEIGHBOUR_PUBKEY_PREFIX_BYTES,
        0xaabbccddu));

    EXPECT_EQ(req[0], sigurdos::mesh::REQ_TYPE_GET_NEIGHBOURS);
    EXPECT_EQ(req[1], sigurdos::mesh::NEIGHBOUR_REQUEST_VERSION);
    EXPECT_EQ(req[2], sigurdos::mesh::NEIGHBOUR_DEFAULT_COUNT);
    EXPECT_EQ(req[3], 0x34);
    EXPECT_EQ(req[4], 0x12);
    EXPECT_EQ(req[5], sigurdos::mesh::NEIGHBOUR_DEFAULT_ORDER_NEWEST);
    EXPECT_EQ(req[6], sigurdos::mesh::NEIGHBOUR_PUBKEY_PREFIX_BYTES);
    EXPECT_EQ(req[7], 0xdd);
    EXPECT_EQ(req[8], 0xcc);
    EXPECT_EQ(req[9], 0xbb);
    EXPECT_EQ(req[10], 0xaa);

    EXPECT_FALSE(sigurdos::mesh::buildNeighboursRequest(
        req, 10,
        sigurdos::mesh::NEIGHBOUR_DEFAULT_COUNT,
        0,
        sigurdos::mesh::NEIGHBOUR_DEFAULT_ORDER_NEWEST,
        sigurdos::mesh::NEIGHBOUR_PUBKEY_PREFIX_BYTES,
        0));
}

TEST(MeshContractTest, NeighboursResponseParserReadsPrefixAgeAndSnr) {
    uint8_t resp[] = {
        0x78, 0x56, 0x34, 0x12, // tag
        0x03, 0x00,             // total neighbours
        0x02, 0x00,             // returned neighbours
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
        0x2c, 0x01, 0x00, 0x00, // 300 seconds ago
        0x18,                   // 6.0 dB
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x05, 0x00, 0x00, 0x00, // 5 seconds ago
        0xf8                    // -2.0 dB
    };

    sigurdos::mesh::NodeNeighboursResult out{};
    ASSERT_TRUE(sigurdos::mesh::parseNeighboursResponse(
        resp, sizeof(resp), sigurdos::mesh::NEIGHBOUR_PUBKEY_PREFIX_BYTES, &out));

    EXPECT_EQ(out.total, 3);
    EXPECT_EQ(out.returned, 2);
    ASSERT_EQ(out.n_items, 2);
    EXPECT_STREQ(out.items[0].pubkey_prefix, "AABBCCDDEEFF");
    EXPECT_EQ(out.items[0].heard_secs_ago, 300u);
    EXPECT_EQ(out.items[0].snr_quarters, 24);
    EXPECT_STREQ(out.items[1].pubkey_prefix, "010203040506");
    EXPECT_EQ(out.items[1].heard_secs_ago, 5u);
    EXPECT_EQ(out.items[1].snr_quarters, -8);

    EXPECT_FALSE(sigurdos::mesh::parseNeighboursResponse(
        resp, 7, sigurdos::mesh::NEIGHBOUR_PUBKEY_PREFIX_BYTES, &out));
}

TEST(MeshContractTest, RoomMessageFormattingUsesPlainPublicPost) {
    char out[64];
    EXPECT_TRUE(sigurdos::mesh::formatRoomMessageText("Public", "hello",
                                                      out, sizeof(out)));
    EXPECT_STREQ(out, "hello");

    EXPECT_TRUE(sigurdos::mesh::formatRoomMessageText("#Public", "hello",
                                                      out, sizeof(out)));
    EXPECT_STREQ(out, "hello");

    EXPECT_TRUE(sigurdos::mesh::formatRoomMessageText("#general", "hello",
                                                      out, sizeof(out)));
    EXPECT_STREQ(out, "#general hello");

    EXPECT_FALSE(sigurdos::mesh::formatRoomMessageText("#", "hello",
                                                       out, sizeof(out)));
    EXPECT_FALSE(sigurdos::mesh::formatRoomMessageText("Public", "hello",
                                                       out, 4));
}

TEST(MeshContractTest, UnsupportedRoomFetchFailsClosedAtWrapper) {
    EXPECT_FALSE(sigurdos::mesh::roomMessageFetchSupported());
}

TEST(MeshContractTest, ActiveRoomServerContextCanBeSetAndCleared) {
    sigurdos::mesh::clearActiveRoomServer();
    EXPECT_STREQ(sigurdos::mesh::getActiveRoomServer(), "");

    EXPECT_TRUE(sigurdos::mesh::setActiveRoomServer("Krabs Lagoon"));
    EXPECT_STREQ(sigurdos::mesh::getActiveRoomServer(), "Krabs Lagoon");

    sigurdos::mesh::clearActiveRoomServer();
    EXPECT_STREQ(sigurdos::mesh::getActiveRoomServer(), "");
    EXPECT_FALSE(sigurdos::mesh::setActiveRoomServer(""));
}

TEST(MeshContractTest, ChannelSendWrappersRejectNullInputs) {
    EXPECT_FALSE(sigurdos::mesh::sendChannelMessage(nullptr, "hello"));
    EXPECT_FALSE(sigurdos::mesh::sendChannelMessage("Public", nullptr));
    EXPECT_FALSE(sigurdos::mesh::sendChannelMessage("", "hello"));
    EXPECT_FALSE(sigurdos::mesh::sendChannelMessage("Public", ""));
    EXPECT_FALSE(sigurdos::mesh::sendChannelMessageWithScopeKey(nullptr, "hello", nullptr));
}

TEST(MeshContractTest, RoomMessageFormattingHonorsRoomServerPostLimit) {
    EXPECT_EQ(sigurdos::mesh::ROOM_SERVER_MAX_POST_TEXT_BYTES, 151u);
    EXPECT_EQ(sigurdos::mesh::roomMessagePrefixBytes("Public"), 0u);
    EXPECT_EQ(sigurdos::mesh::roomMessageMaxBodyBytes("Public"), 151u);

    char body[153];
    std::memset(body, 'a', 151);
    body[151] = '\0';

    char out[160];
    EXPECT_TRUE(sigurdos::mesh::formatRoomMessageText("Public", body,
                                                      out, sizeof(out)));
    EXPECT_EQ(std::strlen(out), sigurdos::mesh::ROOM_SERVER_MAX_POST_TEXT_BYTES);

    body[151] = 'b';
    body[152] = '\0';
    EXPECT_FALSE(sigurdos::mesh::formatRoomMessageText("Public", body,
                                                       out, sizeof(out)));

    EXPECT_EQ(sigurdos::mesh::roomMessageMaxBodyBytes(
                  "#abcdefghijklmnopqrstuvwxyz1234"), 119u);
}

TEST(MeshContractTest, RoomMessageParsingRoutesPublicAndHashtagChannels) {
    char channel[32];
    const char* body = nullptr;

    EXPECT_TRUE(sigurdos::mesh::parseRoomMessageText("#Public hello",
                                                     channel, sizeof(channel), &body));
    EXPECT_STREQ(channel, "Public");
    ASSERT_NE(body, nullptr);
    EXPECT_STREQ(body, "hello");

    EXPECT_TRUE(sigurdos::mesh::parseRoomMessageText("#general  hello",
                                                     channel, sizeof(channel), &body));
    EXPECT_STREQ(channel, "#general");
    ASSERT_NE(body, nullptr);
    EXPECT_STREQ(body, "hello");

    EXPECT_FALSE(sigurdos::mesh::parseRoomMessageText("Public hello",
                                                      channel, sizeof(channel), &body));
    EXPECT_FALSE(sigurdos::mesh::parseRoomMessageText("#Public",
                                                      channel, sizeof(channel), &body));
    EXPECT_FALSE(sigurdos::mesh::parseRoomMessageText("##bad hello",
                                                      channel, sizeof(channel), &body));
}

TEST(MeshContractTest, MessageAndContactBuffersKeepUiCapacities) {
    EXPECT_EQ(sizeof(MeshMessage::sender), 32u);
    EXPECT_EQ(sizeof(MeshMessage::channel), 37u);
    EXPECT_EQ(sizeof(MeshMessage::text), 256u);

    EXPECT_EQ(sizeof(ContactInfo::name), 32u);
    EXPECT_EQ(sizeof(PacketLogEntry::source), 32u);
    EXPECT_EQ(sizeof(PacketLogEntry::type), 16u);
}

TEST(MeshContractTest, NodeStatusWireSizeMatchesDeclaredResponseSize) {
    EXPECT_EQ(NODE_STATUS_RESPONSE_SIZE, 56);
    EXPECT_EQ(sizeof(NodeStatus), static_cast<std::size_t>(NODE_STATUS_RESPONSE_SIZE));
}

TEST(MeshContractTest, NodeStatusResponseParserReadsFullTaggedRepeaterStatsBlob) {
    uint8_t resp[4 + NODE_STATUS_RESPONSE_SIZE] = {0};
    resp[0] = 0x44;
    resp[1] = 0x33;
    resp[2] = 0x22;
    resp[3] = 0x11;

    uint8_t* blob = resp + 4;
    std::size_t pos = 0;
    auto w16 = [&](uint16_t value) {
        std::memcpy(blob + pos, &value, sizeof(value));
        pos += sizeof(value);
    };
    auto wi16 = [&](int16_t value) {
        std::memcpy(blob + pos, &value, sizeof(value));
        pos += sizeof(value);
    };
    auto w32 = [&](uint32_t value) {
        std::memcpy(blob + pos, &value, sizeof(value));
        pos += sizeof(value);
    };

    w16(4120);        // batt_milli_volts
    w16(3);           // curr_tx_queue_len
    wi16(-117);       // noise_floor
    wi16(-83);        // last_rssi
    w32(101);         // n_packets_recv
    w32(202);         // n_packets_sent
    w32(303);         // total_air_time_secs
    w32(404);         // total_up_time_secs
    w32(505);         // n_sent_flood
    w32(606);         // n_sent_direct
    w32(707);         // n_recv_flood
    w32(808);         // n_recv_direct
    w16(9);           // err_events
    wi16(28);         // last_snr
    w16(10);          // n_direct_dups
    w16(11);          // n_flood_dups
    w32(909);         // total_rx_air_time_secs
    w32(1001);        // n_recv_errors
    ASSERT_EQ(pos, static_cast<std::size_t>(NODE_STATUS_RESPONSE_SIZE));

    NodeStatus out{};
    ASSERT_TRUE(sigurdos::mesh::parseNodeStatusResponse(resp, sizeof(resp), &out));
    EXPECT_EQ(out.batt_milli_volts, 4120);
    EXPECT_EQ(out.curr_tx_queue_len, 3);
    EXPECT_EQ(out.noise_floor, -117);
    EXPECT_EQ(out.last_rssi, -83);
    EXPECT_EQ(out.n_packets_recv, 101u);
    EXPECT_EQ(out.n_packets_sent, 202u);
    EXPECT_EQ(out.total_rx_air_time_secs, 909u);
    EXPECT_EQ(out.n_recv_errors, 1001u);

    EXPECT_FALSE(sigurdos::mesh::parseNodeStatusResponse(resp, 4, &out));
    EXPECT_FALSE(sigurdos::mesh::parseNodeStatusResponse(nullptr, sizeof(resp), &out));
    EXPECT_FALSE(sigurdos::mesh::parseNodeStatusResponse(resp, sizeof(resp), nullptr));
}

TEST(MeshContractTest, TelemetryResultKeepsFixedItemCapacity) {
    EXPECT_EQ(MAX_TELEMETRY_ITEMS, 12);
    EXPECT_EQ(sizeof(TelemetryItem::value_str), 24u);
    EXPECT_EQ(sizeof(TelemetryResult::items) / sizeof(TelemetryResult::items[0]),
              static_cast<std::size_t>(MAX_TELEMETRY_ITEMS));
}

TEST(MeshContractTest, GroupDataTypesMatchDocumentedWireValues) {
    EXPECT_EQ(static_cast<uint16_t>(GroupDataType::GDT_NONE), 0x0000);
    EXPECT_EQ(static_cast<uint16_t>(GroupDataType::GDT_TEMPERATURE), 0x0001);
    EXPECT_EQ(static_cast<uint16_t>(GroupDataType::GDT_HUMIDITY), 0x0002);
    EXPECT_EQ(static_cast<uint16_t>(GroupDataType::GDT_PRESSURE), 0x0003);
    EXPECT_EQ(static_cast<uint16_t>(GroupDataType::GDT_LOCATION), 0x0004);
    EXPECT_EQ(static_cast<uint16_t>(GroupDataType::GDT_BATTERY), 0x0005);
    EXPECT_EQ(static_cast<uint16_t>(GroupDataType::GDT_STATUS), 0x0006);
    EXPECT_EQ(static_cast<uint16_t>(GroupDataType::GDT_CUSTOM), 0x00FF);
}

} // namespace
