/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#pragma once

#include "producer.h"
#include "base/algorithm.h"
#include "base/assertion.h"

namespace rpl {

template <typename Value>
class event_stream {
public:
	event_stream();
	event_stream(event_stream &&other);

	void fire(Value value);
	producer<Value, no_error> events() const;

	~event_stream();

private:
	std::weak_ptr<std::vector<consumer<Value, no_error>>> weak() const {
		return _consumers;
	}

	std::shared_ptr<std::vector<consumer<Value, no_error>>> _consumers;

};

template <typename Value>
event_stream<Value>::event_stream()
	: _consumers(std::make_shared<std::vector<consumer<Value, no_error>>>()) {
}

template <typename Value>
event_stream<Value>::event_stream(event_stream &&other)
	: _consumers(base::take(other._consumers)) {
}

template <typename Value>
void event_stream<Value>::fire(Value value) {
	Expects(_consumers != nullptr);
	base::push_back_safe_remove_if(*_consumers, [&](auto &consumer) {
		return !consumer.put_next(value);
	});
}

template <typename Value>
producer<Value, no_error> event_stream<Value>::events() const {
	return producer<Value, no_error>([weak = weak()](consumer<Value, no_error> consumer) {
		if (auto strong = weak.lock()) {
			auto result = [weak, consumer] {
				if (auto strong = weak.lock()) {
					auto it = base::find(*strong, consumer);
					if (it != strong->end()) {
						it->terminate();
					}
				}
			};
			strong->push_back(std::move(consumer));
			return lifetime(std::move(result));
		}
		return lifetime();
	});
}

template <typename Value>
event_stream<Value>::~event_stream() {
	if (_consumers) {
		for (auto &consumer : *_consumers) {
			consumer.put_done();
		}
	}
}

} // namespace rpl