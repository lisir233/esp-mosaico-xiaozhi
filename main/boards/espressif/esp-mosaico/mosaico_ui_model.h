#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>

namespace mosaico {

// Bounded RAM-only history. Never persists microphone transcripts.
class History {
public:
    struct Message {
        std::string role;
        std::string text;
    };
    static constexpr size_t kMaxMessages = 20;
    static constexpr size_t kMaxBytes = 2048;
    const std::deque<Message>& messages() const { return messages_; }
    bool Add(const char* role, const char* text) {
        if (!role || !text || !*text)
            return false;
        std::string value(text);
        if (value.size() > kMaxBytes) {
            size_t end = kMaxBytes;
            while ((static_cast<unsigned char>(value[end]) & 0xc0) == 0x80)
                --end;
            value.resize(end);
            value += "…";
        }
        if (!messages_.empty() && messages_.back().role == role && messages_.back().text == value)
            return false;
        messages_.push_back({role, std::move(value)});
        if (messages_.size() > kMaxMessages)
            messages_.pop_front();
        return true;
    }
    void Clear() { messages_.clear(); }
    bool ClearSystem() {
        return std::erase_if(messages_,
                             [](const Message& message) { return message.role == "system"; }) != 0;
    }
    bool Remove(const char* role, const char* text) {
        if (!role || !text)
            return false;
        for (auto it = messages_.begin(); it != messages_.end(); ++it) {
            if (it->role == role && it->text == text) {
                messages_.erase(it);
                return true;
            }
        }
        return false;
    }

private:
    std::deque<Message> messages_;
};

// Detect motion after settling; rearm only after another quiet interval.
// Units: g and monotonic milliseconds. Motor vibration is blanked by the owner.
class PickupDetector {
public:
    bool Update(float x, float y, float z, uint64_t now) {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return false;
        if (!initialized_) {
            px_ = x;
            py_ = y;
            pz_ = z;
            quiet_since_ = now;
            initialized_ = true;
            return false;
        }
        const float delta =
            std::sqrt((x - px_) * (x - px_) + (y - py_) * (y - py_) + (z - pz_) * (z - pz_));
        px_ = x;
        py_ = y;
        pz_ = z;
        if (delta < 0.045f) {
            if (now - quiet_since_ >= 1500)
                armed_ = true;
        } else {
            quiet_since_ = now;
        }
        if (armed_ && delta > 0.18f && now >= cooldown_until_) {
            armed_ = false;
            cooldown_until_ = now + 5000;
            return true;
        }
        return false;
    }
    void Suppress(uint64_t now) {
        initialized_ = false;
        armed_ = false;
        cooldown_until_ = now + 1500;
    }

private:
    bool initialized_ = false, armed_ = false;
    float px_ = 0, py_ = 0, pz_ = 0;
    uint64_t quiet_since_ = 0, cooldown_until_ = 0;
};
}  // namespace mosaico
