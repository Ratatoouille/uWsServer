#include <iostream>
#include <map>
#include <string>
#include <uwebsockets/App.h>

// ws = new WebSocket("ws://127.0.0.1:9001/");ws.onmessage = ({data}) => console.log("From server: ", data);

const std::string MESSAGE_TO = "message_to::";
const std::string SET_NAME = "set_name::";
const std::string OFFLINE = "offline::";
const std::string ONLINE = "online::";
const std::string BROADCAST_CHANNEL = "broadcast";

// имена пользователей
std::map<std::uint32_t, std::string> userNames;

// данные пользователя
class PerSocketData {
public:
	std::string name;
	std::uint32_t user_id;
};

// обновление имени пользователя
void updateName(PerSocketData* data) {
	userNames[data->user_id] = data->name;
}

// удаление имени пользователя
void deleteName(PerSocketData* data) {
	userNames.erase(data->user_id);
}

// online::id::name
std::string online(std::uint32_t user_id) {
	std::string name = userNames[user_id];
	return ONLINE + std::to_string(user_id) + "::" + name;
}

// offline::id::name
std::string offline(std::uint32_t user_id) {
	std::string name = userNames[user_id];
	return OFFLINE + std::to_string(user_id) + "::" + name;
}

// проверка id
bool isValidId(std::uint32_t last_user_id, std::uint32_t user_id) {
	return user_id > 0 && user_id <= last_user_id;
}

// установка имени
bool isSetName(std::string message) {
	return message.find(SET_NAME) == 0;
}

// проверка имени
bool isValidName(std::string message) {
	return message.find("::") == -1 && message.length() <= 255;
}

// парсинг имени
std::string parseName(std::string message) {
	return message.substr(SET_NAME.size());
}

// парсинг id пользователя
std::string parseUserId(std::string message) {
	std::string buf = message.substr(MESSAGE_TO.size());
	int pos = buf.find("::");
	return buf.substr(0, pos);
}

// парсинг текста сообщения
std::string parseUserText(std::string message) {
	std::string buf = message.substr(MESSAGE_TO.size());
	int pos = buf.find("::");
	return buf.substr(pos + 2);
}

// адресовано сообщение или нет
bool isMessageTo(std::string message) {
	return message.find("message_to::") == 0;
}

// сообщение от пользователя с id
std::string messageFrom(std::string user_id, std::string transmitterName, std::string message) {
	return "message_from::" + user_id + "::(" + transmitterName + ") " + message;
}

int main()
{
	// id последнего пользователя
	std::uint32_t last_user_id = 1;

	// настройка сервера
	uWS::App().ws<PerSocketData>("/*", {
		// время после отключения пользователя в С
		.idleTimeout = 1000,
		// вызывается при подключении пользователя. получение доступа к переменной "last_user_id"
	   .open = [&last_user_id](auto* ws) {

		// получение данных из ws
		PerSocketData* userData = (PerSocketData*)ws->getUserData();

		// назначение уникальных данных пользователю
		userData->name = "USER";
		userData->user_id = last_user_id++;

		// уведомляем обо всех подключенных пользователях
		for (auto entry : userNames) {
			ws->send(online(entry.first), uWS::OpCode::TEXT);
		}

		updateName(userData);
		ws->publish(BROADCAST_CHANNEL, online(userData->user_id));

		std::cout << "New user connected, id = " << userData->user_id << std::endl;
		// вывод числа подключенных пользователей
		std::cout << "Total users connected: " << userNames.size() << std::endl;

		// подписка пользователя на личный канал
		std::string user_chanel = "user#" + std::to_string(userData->user_id);
		ws->subscribe(user_chanel);
		// подписка пользователя на общий канал
		ws->subscribe(BROADCAST_CHANNEL);
	},
		// вызывется при получении сообщения от пользователя
		.message = [&last_user_id](auto* ws, std::string_view message, uWS::OpCode opCode) {
		// обработка сообщения
		PerSocketData* userData = (PerSocketData*)ws->getUserData();
		std::string strMessage = std::string(message);
		std::string transmitterId = std::to_string(userData->user_id);

		// подготовка данных
		if (isMessageTo(strMessage)) {
			std::string receiverId = parseUserId(strMessage);
			std::string text = parseUserText(strMessage);
			std::string outgoingMessage = messageFrom(transmitterId, userData->name, text);

			// проверка id получателя
			if (isValidId(userNames.size(), std::stoi(receiverId))) {
				// отправка получателю
				ws->publish("user#" + receiverId, outgoingMessage, uWS::OpCode::TEXT, false);

				std::cout << "author #" << transmitterId << " sent message to " << receiverId << std::endl;
			}
			else {
				ws->send("Error, there is no user with ID = " + receiverId, uWS::OpCode::TEXT);
				std::cout << "Error, there is no user with ID = " << receiverId << std::endl;
			}
		}

		if (isSetName(strMessage)) {
			std::string newName = parseName(strMessage);
			if (isValidName(newName)) {
				userData->name = newName;
				updateName(userData);
				ws->publish(BROADCAST_CHANNEL, online(userData->user_id));

				std::cout << "User #" << transmitterId << " set his name" << std::endl;
			}
			else std::cout << "User #" << transmitterId << " INVALID NAME!" << std::endl;
		}

	 },
		// вызывается при отключении пользователя
		.close = [](auto* ws, int code, std::string_view message) {
			PerSocketData* userData = (PerSocketData*)ws->getUserData();
			ws->publish(BROADCAST_CHANNEL, offline(userData->user_id));
			deleteName(userData);
			std::cout << "Total users connected: " << userNames.size() << std::endl;
		}
		 // прослушивание порта
		}).listen(9001, [](auto* listen_socket) {
			if (listen_socket) {
				std::cout << "Listening on port " << 9001 << std::endl;
			}
		}).run();
}
