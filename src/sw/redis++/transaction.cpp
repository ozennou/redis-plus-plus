/**************************************************************************
   Copyright (c) 2017 sewenew

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 *************************************************************************/

#include "transaction.h"

namespace sw {

namespace redis {

namespace detail {

std::deque<ReplyUPtr> TransactionImpl::exec(Connection &connection, std::size_t cmd_num) {
    _close_transaction();

    if (_piped) {
        // Get all QUEUED reply
        while (cmd_num > 0) {
            _get_queued_reply(connection);

            --cmd_num;
        }
    }

    return _exec(connection);
}

void TransactionImpl::discard(Connection &connection) {
    _close_transaction();

    cmd::discard(connection);
    auto reply = connection.recv();
    reply::expect_ok_status(*reply);
}

void TransactionImpl::_open_transaction(Connection &connection) {
    assert(!_in_transaction);

    cmd::multi(connection);
    auto reply = connection.recv();
    auto status = reply::to_status(*reply);
    if (status != "OK") {
        throw Error("Failed to open transaction: " + status);
    }

    _in_transaction = true;
}

void TransactionImpl::_close_transaction() {
    if (!_in_transaction) {
        throw Error("No command in transaction");
    }

    _in_transaction = false;
}

void TransactionImpl::_get_queued_reply(Connection &connection) {
    auto reply = connection.recv();
    auto status = reply::to_status(*reply);
    if (status != "QUEUED") {
        throw Error("Invalid QUEUED reply: " + status);
    }
}

std::deque<ReplyUPtr> TransactionImpl::_exec(Connection &connection) {
    cmd::exec(connection);

    auto reply = connection.recv();
    if (!reply::is_array(*reply)) {
        throw ProtoError("Expect ARRAY reply");
    }

    if (reply->element == nullptr || reply->elements == 0) {
        // In fact, this should never happen
        assert(false);

        return {};
    }

    std::deque<ReplyUPtr> replies;
    for (std::size_t idx = 0; idx != reply->elements; ++idx) {
        auto *sub_reply = reply->element[idx];
        if (sub_reply == nullptr) {
            throw ProtoError("Null sub reply");
        }

        auto r = ReplyUPtr(sub_reply);
        reply->element[idx] = nullptr;
        replies.push_back(std::move(r));
    }

    return replies;
}

}

}

}