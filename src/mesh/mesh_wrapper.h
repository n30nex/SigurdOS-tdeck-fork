// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// MeshCore protocol integration for SigurdOS.
// Uses SigurdMesh, a minimal mesh::Mesh subclass.

#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <helpers/RegionMap.h>  // for RegionEntry (must be before namespace)
#include "public_channel.h"

// Node type identifiers from MeshCore adverts — kept local so UI code can filter.
#define ADV_TYPE_NONE      0
#define ADV_TYPE_CHAT      1
#define ADV_TYPE_REPEATER  2
#define ADV_TYPE_ROOM      3
#define ADV_TYPE_SENSOR    4

// ACL permission levels for contacts (stored in ContactInfo.perm and
// persisted in the contacts file). Packed into ContactInfo::flags bits 1-2.
#define PERM_ACL_GUEST      0
#define PERM_ACL_READ_ONLY  1
#define PERM_ACL_READ_WRITE 2
#define PERM_ACL_ADMIN      3

namespace sigurdos {
namespace mesh {

namespace detail {
static constexpr uint32_t CONTACT_PUBLIC_INDEX_OFFSET = 8;

inline uint32_t contactPublicIndexToRawSlot(uint32_t public_index)
{
    return public_index + CONTACT_PUBLIC_INDEX_OFFSET;
}
} // namespace detail

// Forward declarations from mesh_wrapper.cpp
// sender_timestamp / path_len carry the originating packet's stamp and mesh
// path-length byte through to the companion bridge so the phone app shows the
// correct time and hop count. Defaults (0 / 0xFF) suit callers without a packet:
// the timestamp falls back to the local clock and 0xFF means "direct/unknown".
// sender_prefix (6 bytes) is the exact pubkey prefix from packet metadata —
// null falls back to a name-based lookup. txt_type, extra, and extra_len
// preserve the MeshCore companion text type and signed-message extra.
void mesh_v2_queue_push(const char* sender, const char* channel,
                         const char* text, int rssi, float snr,
                         uint32_t sender_timestamp = 0, uint8_t path_len = 0xFF,
                         const uint8_t* sender_prefix = nullptr,
                         uint8_t txt_type = 0,
                         const uint8_t* extra = nullptr,
                         uint8_t extra_len = 0);

// Forwards a delivery ACK to the companion bridge so the phone app marks a
// message it sent (via the device) as confirmed. ack is the 4-byte ACK hash the
// app received in RESP_CODE_SENT; trip_time_ms is the round-trip time.
void mesh_v2_notify_send_confirmed(uint32_t ack, uint32_t trip_time_ms);
void mesh_v2_group_data_push(uint8_t channel_index,
                              uint8_t path_len,
                              int8_t snr_quarters,
                              uint16_t data_type,
                              const uint8_t* data,
                              size_t data_len);
void mesh_v2_note_contact_activity(uint8_t contact_type, bool is_new_visible_contact);

// Live mesh-event fan-out to the companion bridge (no-ops when the bridge is
// absent / no phone connected). Primitive args keep this header MeshCore-free.
// contact_info is an opaque `const ::ContactInfo*` (kept void* so this header
// stays free of MeshCore types); the adapter casts it back.
void mesh_v2_companion_advert_push(const void* contact_info, bool is_new);
void mesh_v2_companion_path_push(const uint8_t* pub_key);
void mesh_v2_companion_contact_deleted_push(const uint8_t* pub_key);
void mesh_v2_companion_contacts_full_push();
void mesh_v2_companion_login_push(const uint8_t* pub_key, bool success,
                                  uint8_t permission, bool is_admin,
                                  uint32_t server_tag = 0,
                                  uint8_t acl = 0,
                                  uint8_t firmware_level = 0,
                                  bool include_extended = false);
void mesh_v2_companion_status_push(const uint8_t* pub_key, const uint8_t* blob, size_t len);
void mesh_v2_companion_telemetry_push(const uint8_t* pub_key, const uint8_t* blob, size_t len);
void mesh_v2_companion_trace_push(uint32_t tag, uint32_t auth, uint8_t flags,
                                  const uint8_t* path_hashes, const uint8_t* path_snrs,
                                  uint8_t path_len, int8_t final_snr_quarters);

struct MeshMessage {
    char sender[32];
    // Matches the chat UI conversation cap so synthetic entries such as
    // "Room:" + a 31-byte contact name are not truncated in the runtime queue.
    char channel[37];
    char text[256];
    uint32_t timestamp;
    // MeshCore companion TXT_TYPE_* value. 0 = plain, 1 = CLI data,
    // 2 = signed plain. The UI uses this to keep command replies distinct.
    uint8_t txt_type;
    bool is_self;
};

struct ContactInfo {
    char name[32];
    uint8_t type;  // ADV_TYPE_* (ADV_TYPE_CHAT=companion, ADV_TYPE_REPEATER, ADV_TYPE_ROOM, etc.)
    uint8_t perm;  // PERM_ACL_* (0=guest, 1=read-only, 2=read-write, 3=admin), default 0
    bool has_location;
    float latitude;
    float longitude;
    int  rssi;
    float snr;
    uint32_t last_seen;
};

struct PacketLogEntry {
    uint32_t timestamp;
    char     source[32];
    int      rssi;
    float    snr;
    char     type[16];
};

inline bool autoAddConfigAllowsContactType(uint8_t type, uint8_t config)
{
    if (type < ADV_TYPE_CHAT || type > ADV_TYPE_SENSOR) return false;
    return (config & (1u << type)) != 0;
}

inline bool autoAddConfigAllowsOverwriteOldest(uint8_t config)
{
    return (config & 0x01u) != 0;
}

static constexpr uint16_t LOGIN_DEFAULT_KEEP_ALIVE_SECS = 60;
static constexpr size_t ROOM_SERVER_MAX_POST_TEXT_BYTES = 151;

inline uint16_t loginKeepAliveSeconds(uint8_t raw_units, uint8_t contact_type)
{
    uint16_t secs = static_cast<uint16_t>(raw_units) * 16u;
    if (secs == 0 &&
        (contact_type == ADV_TYPE_REPEATER || contact_type == ADV_TYPE_ROOM)) {
        return LOGIN_DEFAULT_KEEP_ALIVE_SECS;
    }
    return secs;
}

enum class LoginResponseKind : uint8_t {
    Ignored = 0,
    NewOk,
    LegacyOk,
    Failed,
};

struct LoginResponseParseResult {
    LoginResponseKind kind = LoginResponseKind::Ignored;
    uint8_t keep_alive_units = 0;
    uint8_t permission = 0;
    uint8_t acl = 0;
    uint32_t server_tag = 0;
    uint8_t firmware_level = 0;
    uint8_t failure_code = 0;
};

inline LoginResponseParseResult parseLoginResponse(const uint8_t* data,
                                                   size_t len,
                                                   bool login_pending)
{
    LoginResponseParseResult result{};
    if (!data || len < 5) return result;

    if (data[4] == 0) {
        result.kind = LoginResponseKind::NewOk;
        std::memcpy(&result.server_tag, data, 4);
        result.keep_alive_units = (len > 5) ? data[5] : 0;
        result.permission = (len > 6) ? data[6] : 0;
        result.acl = (len > 7) ? data[7] : 0;
        result.firmware_level = (len > 12) ? data[12] : 0;
        return result;
    }

    if (len >= 6 && data[4] == 'O' && data[5] == 'K') {
        result.kind = LoginResponseKind::LegacyOk;
        result.permission = 1;
        return result;
    }

    if (login_pending && data[4] != 0) {
        result.kind = LoginResponseKind::Failed;
        result.failure_code = data[4];
    }
    return result;
}

inline size_t roomMessagePrefixBytes(const char* channel_name)
{
    if (!channel_name) return 0;
    const char* ch = channel_name;
    while (*ch == '#') ++ch;
    if (!*ch) return 0;
    if (std::strcmp(ch, PUBLIC_CHANNEL_NAME) == 0) return 0;
    return 2u + std::strlen(ch);  // '#' + channel + ' '
}

bool init(bool spiffs_ok = true);
void loop();

// Returns 0 on failure, or the epoch-second timestamp the mesh layer used for ACK tracking.
// The UI must store this returned timestamp so isMessageAcked() can match against it later.
uint32_t sendMessage(const char* dest_name, const char* text);
bool sendChannelMessage(const char* channel_name, const char* text);
// Scoped variants temporarily stamp this send with key16. A null key sends unscoped.
uint32_t sendMessageWithScopeKey(const char* dest_name, const char* text, const uint8_t* key16);
bool sendChannelMessageWithScopeKey(const char* channel_name, const char* text, const uint8_t* key16);

int  pollMessages(MeshMessage* out, int max);
int  pendingMessageCount();
uint32_t getQueueDropCount();
int  getUnreadMessageCount();
void resetUnreadMessageCount();
int  getUnreadChannelMessageCount();
void resetUnreadChannelMessageCount();
int  getUnreadDmMessageCount();
void resetUnreadDmMessageCount();
int  getUnreadContactCount();
void resetUnreadContactCount();
int  getUnreadRepeaterCount();
void resetUnreadRepeaterCount();
uint32_t getMeshActivitySeq();

int  getContactCount();
int  exportContacts(char names[][32], int max);
int  exportContactsFull(ContactInfo* out, int max);
bool getContactByName(const char* name, ContactInfo* out);
bool isContactFavourite(const char* name);
void setContactFavourite(const char* name, bool favourite);

int  getChannelCount();
int  exportChannels(char names[][37], int max);
bool addChannel(const char* name, const char* psk_base64);
bool addHashtagChannel(const char* name);
bool joinPublicChannel();

void setOwnName(const char* name);
const char* getOwnName();

int   getNoiseFloor();
int   getLastRSSI();
float getLastSNR();
unsigned long getTotalTxAirtimeMs();
unsigned long getTotalRxAirtimeMs();
uint32_t getNumSentFlood();
uint32_t getNumSentDirect();
uint32_t getNumRecvFlood();
uint32_t getNumRecvDirect();
void resetPacketStats();

bool sendAdvert();
uint32_t getLastAdvertTime();
bool     getLastAdvertSuccess();
bool     getLastAdvertUsedGps();
void saveState();
bool saveChannels();
void loadChannels();
void shutdown();
void factoryReset();

// Companion BLE bridge
bool companionBleAvailable();
bool companionBleSetEnabled(bool enabled);
bool companionBleEnabled();
bool companionBleConnected();
uint32_t companionBleLastSyncTime();
uint32_t companionBlePin();

// ── Contact persistence ─────────────────────────
void saveContacts();
void loadContacts();
void reloadContactsAfterIdentityChange();  // after private key import

// RTC time for UI comparisons
uint32_t getCurrentTime();
uint32_t getCurrentTimeUnique();
bool setSystemTime(uint32_t epoch_seconds);

void getCurrentLocalDateTime(int* year, int* month, int* day, int* hour, int* minute);
uint32_t makeEpoch(int year, int month, int day, int hour, int minute);

// Packet log
int  getPacketLogCount();
bool getPacketLogEntry(int index, PacketLogEntry* out);
void pushPacketLog(const char* source, int rssi, float snr, const char* type);

// Inject a simulated incoming message into the message queue (for remote test mode).
// No radio transmission occurs. The message appears as if received from another node.
void injectMessage(const char* sender, const char* channel, const char* text);

// Trace route
bool sendTrace(int contact_idx, uint32_t* out_tag);
int  findContactIndex(const char* name);
bool hasTraceResult();
uint8_t getTracePathLen();
void   getTracePath(uint8_t* snrs_out, uint8_t* hashes_out);
void   clearTraceResult();
bool   contactHasPath(int contact_idx);

// ── Ping Nearby ─────────────────────────────────
struct PingResult {
    char name[32];
    int rssi;
};
bool     sendPingNearby();
bool     pingIsActive();
bool     pingOnCooldown();
uint32_t pingCooldownRemaining();
uint32_t activePingRemaining();
int      getPingResultCount();
const PingResult* getPingResult(int i);

// ── Signal history for sparkline ─────────────
int  getSignalHistoryCount();
int  getSignalHistoryRSSI(int idx);
float getSignalHistorySNR(int idx);

// ── Live radio config (no NVS write) ──────────
bool applyRadioParams(float freq, float bw, int sf, int cr, int tx_power, bool rx_gain);
bool revertRadioParams();

// ── REQ/RESPONSE framework (Phase 4.1) ────────
bool sendRequest(const char* dest_name, uint8_t req_type);
bool sendRequestWithData(const char* dest_name, const uint8_t* data, uint8_t len);
int  getResponseCount();
bool getResponse(int idx, uint32_t* out_tag, uint8_t* out_data, uint8_t* out_len, char* out_contact_name);
void clearResponses();

// ── Duty cycle ────────────────────────────────
unsigned long getRemainingTxBudget();
void setDutyCycle(uint8_t percent);

// ── Contact management extensions ────────────
bool removeContact(const char* name);
bool resetPathTo(const char* name);
bool setContactPerm(const char* name, uint8_t perm);
int  getContactPerm(const char* name);
bool resetRoomServerSync(const char* name);

// ── Channel management extensions ────────────
bool removeChannel(int idx);

// ── ACK tracking ──────────────────────────────
void registerAckedMessage(const char* dest_name, uint32_t timestamp);
bool isMessageAcked(const char* dest_name, uint32_t timestamp);
int  getAckCounter();   // incremented each time registerAckedMessage is called

// ── Room message fetch (Phase 4.6) ────────────────
inline bool roomMessageFetchSupported() { return false; }

inline bool roomMessageTargetAllowsSend(uint8_t contact_type)
{
    return contact_type == ADV_TYPE_ROOM;
}
bool sendRoomMsgFetchRequest(const char* contact_name, const char* channel_name);
int  getRoomMsgFetchCount();
bool getRoomMsgFetchEntry(int index, char* sender_out, int sender_sz,
                          char* text_out, int text_sz,
                          char* channel_out, int channel_sz,
                          uint32_t* timestamp_out);
void clearRoomMsgFetch();

// Send a text message to a room server contact as a peer TXT_MSG.
// Returns the timestamp used for ACK tracking, or 0 on failure.
inline bool formatRoomMessageText(const char* channel_name, const char* text,
                                  char* out, size_t out_sz) {
    if (!channel_name || !text || !out || out_sz == 0) return false;
    const char* ch = channel_name;
    while (*ch == '#') ++ch;
    if (!*ch) return false;
    const size_t prefix_len = roomMessagePrefixBytes(channel_name);
    const bool public_room = std::strcmp(ch, PUBLIC_CHANNEL_NAME) == 0;
    if (!public_room && (prefix_len == 0 || prefix_len >= ROOM_SERVER_MAX_POST_TEXT_BYTES)) return false;
    const size_t max_body = ROOM_SERVER_MAX_POST_TEXT_BYTES - prefix_len;
    if (std::strlen(text) > max_body) return false;
    int n = public_room
        ? std::snprintf(out, out_sz, "%s", text)
        : std::snprintf(out, out_sz, "#%s %s", ch, text);
    if (n <= 0) {
        out[0] = '\0';
        return false;
    }
    out[out_sz - 1] = '\0';
    return n < static_cast<int>(out_sz) &&
           static_cast<size_t>(n) <= ROOM_SERVER_MAX_POST_TEXT_BYTES;
}

inline size_t roomMessageMaxBodyBytes(const char* channel_name)
{
    if (channel_name) {
        const char* ch = channel_name;
        while (*ch == '#') ++ch;
        if (std::strcmp(ch, PUBLIC_CHANNEL_NAME) == 0) return ROOM_SERVER_MAX_POST_TEXT_BYTES;
    }
    const size_t prefix_len = roomMessagePrefixBytes(channel_name);
    if (prefix_len == 0 || prefix_len >= ROOM_SERVER_MAX_POST_TEXT_BYTES) return 0;
    return ROOM_SERVER_MAX_POST_TEXT_BYTES - prefix_len;
}

inline bool roomPostTextIsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

inline bool parseRoomMessageText(const char* text,
                                 char* channel_out,
                                 size_t channel_out_sz,
                                 const char** body_out)
{
    if (!text || !channel_out || channel_out_sz == 0 || !body_out) return false;
    channel_out[0] = '\0';
    *body_out = nullptr;
    if (text[0] != '#') return false;

    const char* p = text + 1;
    char token[32];
    size_t token_len = 0;
    while (*p && !roomPostTextIsSpace(*p)) {
        if (*p == '#') return false;
        if (token_len + 1 >= sizeof(token)) return false;
        token[token_len++] = *p++;
    }
    token[token_len] = '\0';
    if (token_len == 0) return false;

    while (roomPostTextIsSpace(*p)) ++p;
    if (!*p) return false;

    char normalized[32];
    if (std::strcmp(token, PUBLIC_CHANNEL_NAME) == 0) {
        std::snprintf(normalized, sizeof(normalized), "%s", PUBLIC_CHANNEL_NAME);
    } else {
        std::snprintf(normalized, sizeof(normalized), "#%s", token);
    }
    if (std::strlen(normalized) >= channel_out_sz) return false;
    std::strncpy(channel_out, normalized, channel_out_sz - 1);
    channel_out[channel_out_sz - 1] = '\0';
    *body_out = p;
    return true;
}

uint32_t sendRoomMessage(const char* contact_name, const char* channel_name, const char* text);

bool setActiveRoomServer(const char* contact_name);
void clearActiveRoomServer();
const char* getActiveRoomServer();

// Count room server contacts that are currently logged in.
int getLoggedInRoomServerCount();

// Get the name of a logged-in room server contact by index (0..count-1).
// Returns the contact name or empty string if not found.
const char* getLoggedInRoomServerName(int index);

// ── Status request (Phase 4.2) ────────────────
#define NODE_STATUS_RESPONSE_SIZE  56  // size of RepeaterStats binary blob

struct NodeStatus {
    uint16_t batt_milli_volts;       // battery voltage in mV
    uint16_t curr_tx_queue_len;      // current TX queue length
    int16_t  noise_floor;            // noise floor (dBm)
    int16_t  last_rssi;              // last received RSSI (dBm)
    uint32_t n_packets_recv;         // total packets received
    uint32_t n_packets_sent;         // total packets sent
    uint32_t total_air_time_secs;    // total TX air time (seconds)
    uint32_t total_up_time_secs;     // node uptime (seconds)
    uint32_t n_sent_flood;           // flood messages sent
    uint32_t n_sent_direct;          // direct messages sent
    uint32_t n_recv_flood;           // flood messages received
    uint32_t n_recv_direct;          // direct messages received
    uint16_t err_events;             // error event count
    int16_t  last_snr;               // last SNR (value/4 = dB)
    uint16_t n_direct_dups;          // duplicate direct packets
    uint16_t n_flood_dups;           // duplicate flood packets
    uint32_t total_rx_air_time_secs; // total RX air time (seconds)
    uint32_t n_recv_errors;          // receive errors
};

inline bool parseNodeStatusResponse(const uint8_t* data, uint8_t len, NodeStatus* out)
{
    if (!data || !out || len <= 4) return false;
    std::memset(out, 0, sizeof(*out));
    // Response data is [4-byte tag][RepeaterStats blob]. The stats blob is
    // NODE_STATUS_RESPONSE_SIZE bytes on current MeshCore repeaters.
    const uint8_t* blob = data + 4;
    uint8_t blen = static_cast<uint8_t>(len - 4);
    if (blen > NODE_STATUS_RESPONSE_SIZE) blen = NODE_STATUS_RESPONSE_SIZE;
    if (blen < 2) return false;

    unsigned ofs = 0;
    auto r16 = [&](int16_t* dst) {
        if (ofs + 2 <= blen) {
            std::memcpy(dst, blob + ofs, 2);
            ofs += 2;
        }
    };
    auto ru16 = [&](uint16_t* dst) {
        if (ofs + 2 <= blen) {
            std::memcpy(dst, blob + ofs, 2);
            ofs += 2;
        }
    };
    auto ru32 = [&](uint32_t* dst) {
        if (ofs + 4 <= blen) {
            std::memcpy(dst, blob + ofs, 4);
            ofs += 4;
        }
    };
    ru16(&out->batt_milli_volts);
    ru16(&out->curr_tx_queue_len);
    r16(&out->noise_floor);
    r16(&out->last_rssi);
    ru32(&out->n_packets_recv);
    ru32(&out->n_packets_sent);
    ru32(&out->total_air_time_secs);
    ru32(&out->total_up_time_secs);
    ru32(&out->n_sent_flood);
    ru32(&out->n_sent_direct);
    ru32(&out->n_recv_flood);
    ru32(&out->n_recv_direct);
    ru16(&out->err_events);
    r16(&out->last_snr);
    ru16(&out->n_direct_dups);
    ru16(&out->n_flood_dups);
    ru32(&out->total_rx_air_time_secs);
    ru32(&out->n_recv_errors);
    return true;
}

bool requestStatus(const char* dest_name);
bool hasStatusResponse();
bool getStatusResult(NodeStatus* out);

// ── Telemetry queries (Phase 4.3) ─────────────
#define MAX_TELEMETRY_ITEMS 12

struct TelemetryItem {
    uint8_t channel;
    uint8_t type;
    float   value_float;     // decoded float value (temperature C, voltage V, etc.)
    char    value_str[24];   // formatted string for display
};

struct TelemetryResult {
    int          n_items;
    TelemetryItem items[MAX_TELEMETRY_ITEMS];
};

bool requestTelemetry(const char* dest_name);
bool hasTelemetryResponse();
bool getTelemetryResult(TelemetryResult* out);

// ── Path discovery (Phase 4.4) ────────────────
// Sends a flood request to discover the route to a contact.
// Returns a discovery tag (>0) on success, or 0 on failure.
uint32_t discoverPath(const char* dest_name);
// Check if a path has been learned for a contact (path_len > 0 || path_len == 0xFF unknown)
bool hasPathTo(const char* dest_name);
uint8_t getContactPathLen(const char* dest_name);

// ── Advert path (inbound) ─────────────────────
// Returns the number of hops the advert from this contact traversed
// to reach us. Returns 0 if no advert path is known for this contact.
uint8_t getAdvertPathLen(const char* name);

// ── Login status values ───────────────────────
#define LOGIN_STATUS_NONE    0   // not logged in
#define LOGIN_STATUS_PENDING 1   // login request sent, awaiting response
#define LOGIN_STATUS_OK      2   // logged in successfully
#define LOGIN_STATUS_FAILED  3   // login was rejected
static constexpr uint32_t LOGIN_PENDING_TIMEOUT_MS = 30000UL;

inline bool loginStatusNeedsLocalCancel(uint8_t status) {
    return status == LOGIN_STATUS_PENDING || status == LOGIN_STATUS_FAILED;
}

inline bool loginStatusCanBeReclaimed(uint8_t status) {
    return status == LOGIN_STATUS_NONE || status == LOGIN_STATUS_FAILED;
}

inline bool loginPendingTimedOut(uint32_t now_ms, uint32_t started_at_ms) {
    return (uint32_t)(now_ms - started_at_ms) >= LOGIN_PENDING_TIMEOUT_MS;
}

inline uint8_t effectiveLoginPermission(uint8_t legacy_permission,
                                        uint8_t acl_permissions)
{
    if (acl_permissions != 0) return acl_permissions & 0x03u;
    return legacy_permission == 1 ? PERM_ACL_ADMIN : PERM_ACL_GUEST;
}

static constexpr uint32_t PENDING_REQUEST_TTL_MS = 120000UL;

inline bool pendingRequestExpired(uint32_t sent_at_ms, uint32_t now_ms) {
    return (uint32_t)(now_ms - sent_at_ms) >= PENDING_REQUEST_TTL_MS;
}

// ── Repeater/room login (Phase 4.5) ──────────────
static constexpr size_t LOGIN_PASSWORD_MAX_BYTES = 15;

inline bool loginPasswordInputSubmittable(const char* password) {
    // MeshCore accepts an empty password as an ACL/guest login attempt.
    return password != nullptr && std::strlen(password) <= LOGIN_PASSWORD_MAX_BYTES;
}

inline bool loginPasswordAllowedForContactType(uint8_t contact_type, const char* password) {
    if (!loginPasswordInputSubmittable(password)) return false;
    // MeshCore repeaters and room servers both accept a blank password as an
    // ACL/guest session refresh. It may fail remotely if the node is not in the
    // ACL yet, but it must be allowed through so the UI can show real pending
    // and failure state instead of treating guest access as invalid input.
    return contact_type == ADV_TYPE_REPEATER || contact_type == ADV_TYPE_ROOM;
}

inline bool loginContactTypeMatchesHint(uint8_t contact_type, uint8_t contact_type_hint) {
    if (contact_type_hint == ADV_TYPE_NONE) {
        return contact_type == ADV_TYPE_REPEATER || contact_type == ADV_TYPE_ROOM;
    }
    return contact_type == contact_type_hint;
}

inline bool loginShouldForceFloodForContactType(uint8_t contact_type) {
    // Login is the session bootstrap and path repair point for infrastructure
    // contacts. A stale direct path can strand the UI at Login pending, while
    // MeshCore repeaters/rooms deliberately support flood login responses that
    // return a usable path.
    return contact_type == ADV_TYPE_REPEATER || contact_type == ADV_TYPE_ROOM;
}

inline bool loginBootstrapShouldBypassFloodScope(uint8_t contact_type) {
    // Region/private-scope transport codes are for normal mesh traffic. Login is
    // the bootstrap that discovers a usable repeater/room path, so it must be
    // an unscoped flood even when the current chat region is scoped.
    return loginShouldForceFloodForContactType(contact_type);
}

bool sendLogin(const char* name, const char* password);
bool sendLoginForContactType(const char* name, const char* password, uint8_t contact_type_hint);
void sendLogout(const char* name);
void clearLoginState(const char* name);
bool sendCommand(const char* name, const char* text);
bool isLoggedIn(const char* name);
uint8_t getLoginPermission(const char* name);
uint8_t getLoginStatus(const char* name);
void forceLoginState(const char* name, uint8_t status, uint8_t permission);

// ── Repeater neighbours request ─────────────────────
static constexpr uint8_t REQ_TYPE_GET_NEIGHBOURS = 0x06;
static constexpr uint8_t NEIGHBOUR_REQUEST_VERSION = 0;
static constexpr uint8_t NEIGHBOUR_DEFAULT_COUNT = 8;
static constexpr uint8_t NEIGHBOUR_DEFAULT_ORDER_NEWEST = 0;
static constexpr uint8_t NEIGHBOUR_PUBKEY_PREFIX_BYTES = 6;
static constexpr int MAX_NODE_NEIGHBOURS = 8;

struct NodeNeighbourInfo {
    char pubkey_prefix[(NEIGHBOUR_PUBKEY_PREFIX_BYTES * 2) + 1];
    uint32_t heard_secs_ago;
    int8_t snr_quarters;
};

struct NodeNeighboursResult {
    uint16_t total;
    uint16_t returned;
    int n_items;
    NodeNeighbourInfo items[MAX_NODE_NEIGHBOURS];
};

inline bool buildNeighboursRequest(uint8_t* out,
                                   size_t out_size,
                                   uint8_t count,
                                   uint16_t offset,
                                   uint8_t order_by,
                                   uint8_t pubkey_prefix_len,
                                   uint32_t nonce)
{
    if (!out || out_size < 11 || pubkey_prefix_len == 0 ||
        pubkey_prefix_len > 32 || count == 0) {
        return false;
    }
    out[0] = REQ_TYPE_GET_NEIGHBOURS;
    out[1] = NEIGHBOUR_REQUEST_VERSION;
    out[2] = count;
    std::memcpy(&out[3], &offset, 2);
    out[5] = order_by;
    out[6] = pubkey_prefix_len;
    std::memcpy(&out[7], &nonce, 4);
    return true;
}

inline bool parseNeighboursResponse(const uint8_t* data,
                                    uint8_t len,
                                    uint8_t pubkey_prefix_len,
                                    NodeNeighboursResult* out)
{
    if (!data || !out || pubkey_prefix_len == 0 || pubkey_prefix_len > 32 ||
        len < 8) {
        return false;
    }
    std::memset(out, 0, sizeof(*out));
    size_t pos = 4; // response tag
    std::memcpy(&out->total, &data[pos], 2); pos += 2;
    std::memcpy(&out->returned, &data[pos], 2); pos += 2;

    const size_t entry_size = static_cast<size_t>(pubkey_prefix_len) + 4u + 1u;
    int expected = out->returned;
    if (expected > MAX_NODE_NEIGHBOURS) expected = MAX_NODE_NEIGHBOURS;
    while (out->n_items < expected && pos + entry_size <= len) {
        NodeNeighbourInfo& item = out->items[out->n_items++];
        char* hex = item.pubkey_prefix;
        size_t hex_pos = 0;
        for (uint8_t i = 0; i < pubkey_prefix_len &&
             i < NEIGHBOUR_PUBKEY_PREFIX_BYTES; i++) {
            std::snprintf(hex + hex_pos, sizeof(item.pubkey_prefix) - hex_pos,
                          "%02X", data[pos + i]);
            hex_pos += 2;
        }
        hex[sizeof(item.pubkey_prefix) - 1] = '\0';
        pos += pubkey_prefix_len;
        std::memcpy(&item.heard_secs_ago, &data[pos], 4); pos += 4;
        item.snr_quarters = static_cast<int8_t>(data[pos++]);
    }
    return true;
}

bool requestNeighbours(const char* dest_name);
bool hasNeighboursResponse();
bool getNeighboursResult(NodeNeighboursResult* out);


// ── Command response ring buffer (for terminal UI) ──
#define MAX_CMD_RESPONSES 16
void pushCmdResponse(const char* name, const char* text);
bool pollCmdResponse(char* name_out, int name_sz, char* text_out, int text_sz);
void clearCmdResponses();

#if defined(SIGURDOS_REMOTE_TEST)
// Test helper: inject a fake repeater contact into the mesh contact list.
// The contact will have the given name, type ADV_TYPE_REPEATER, and test SNR/RSSI.
// Used by the test controller to verify the repeater detail UI without real radio traffic.
bool addTestRepeater(const char* name);
bool addTestRoomServer(const char* name);
#endif

// ── Anonymous requests (Phase 4.7) ────────────────
// Send a text message to a node identified by its 64-hex-char public key.
// The node does NOT need to be in your contact list.
bool sendAnonMessage(const char* pubkey_hex, const char* text);

// ── Group data datagrams (Phase 4.8) ─────────────
// Standard data type constants
// (Defined as static constexpr in SigurdMeshV2; reusing here via enum)
enum GroupDataType : uint16_t {
    GDT_NONE        = 0x0000,
    GDT_TEMPERATURE = 0x0001,
    GDT_HUMIDITY    = 0x0002,
    GDT_PRESSURE    = 0x0003,
    GDT_LOCATION    = 0x0004,
    GDT_BATTERY     = 0x0005,
    GDT_STATUS      = 0x0006,
    GDT_CUSTOM      = 0x00FF
};

// Hex-to-bytes helper (used by terminal commands)
int hexToBytes(const char* hex, uint8_t* out, int out_max);

// Send typed data to a group channel by index
bool sendGroupDataToChannel(int channel_idx, uint16_t data_type,
                            const uint8_t* data, int data_len);

// Polling API for received group datagrams
int  getGroupDataRecvCount();
bool getGroupDataRecvEntry(int index, uint16_t* data_type_out,
                           uint8_t* data_out, int data_out_max, int* data_len_out,
                           char* channel_out, int channel_sz,
                           uint32_t* timestamp_out);
void clearGroupDataRecv();

// ── Message signing ──
// Sign arbitrary data with this node's private key.
// Returns number of bytes written to sig_out (SIGNATURE_SIZE=64 on success, 0 on failure).
int signMessage(const char* data, uint8_t* sig_out);

// ── Identity backup (export/import) ──
// Export the node's Ed25519 private key as a hex string (128 hex chars = 64 bytes).
// Returns true on success. hex_out must be at least 129 characters.
bool exportIdentity(char* hex_out, size_t hex_sz);

// Import a 128-hex-char Ed25519 private key (64 bytes), re-keying the node.
// The public key is recomputed from the private key. The identity is saved to
// SPIFFS and the node should be rebooted to reload contacts with new identity.
// Returns true on success if the key is valid.
bool importIdentity(const char* hex_privkey);

// ── URI import ──
// Import a contact from a meshcore:// URI (query-param or raw hex blob format).
// Returns true if a contact was successfully imported.
bool importContactByUri(const char* uri);

// Add a channel from a meshcore://channel/add?... URI.
// Returns true if the channel was successfully added.
bool addChannelByUri(const char* uri);

// ── QR code support ─────────────────────────────
// Look up a contact by name and write its 64-hex-char public key to hex_out.
// hex_sz must be at least PUB_KEY_SIZE * 2 + 1 (65). Returns false if
// g_mesh is null, the contact is not found, or the buffer is too small.
bool getContactPubkeyHex(const char* name, char* hex_out, size_t hex_sz);

// Write the 64-hex-char channel secret for the given channel index to hex_out.
// hex_sz must be at least PUB_KEY_SIZE * 2 + 1 (65). Returns false if
// g_mesh is null, the channel index is out of range, or the buffer is too small.
bool getChannelSecretHex(int channel_idx, char* hex_out, size_t hex_sz);

// ── Regions (flood scope) ──────────────────────────
struct RegionInfo;
// List saved regions. Returns count (≤ max).
int  listRegions(RegionInfo* out, int max);

// Auto-create #regions from #channels.
// Skips channels without a # prefix and regions that already exist.
void syncRegionsFromChannels();

// Add a region. For #public names, transport keys are auto-derived.
// parent_name is optional for hierarchy placement.
/// Returns the RegionEntry or nullptr on failure.
RegionEntry* addRegion(const char* name, const char* parent_name);

// Remove a saved region by name.
bool removeRegion(const char* name);

// Set the active flood-scope region (empty = wildcard/unscoped).
bool setActiveRegion(const char* name);

// Get the current active region name. Returns "" if wildcard.
const char* getActiveRegion();

// Temporarily send the next message unscoped (resets after one use).
void setSendUnscopedOnce(bool v);

// Percent-encode a string for a URL query-component value.
// Returns bytes written (excluding NUL), or 0 on overflow.
// Worst case: every byte becomes %XX (3x expansion + NUL).
size_t urlEncodeQueryValue(const char* in, char* out, size_t out_sz);

} // namespace mesh
} // namespace sigurdos
