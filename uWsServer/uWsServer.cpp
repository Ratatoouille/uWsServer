#include <string>
#include <algorithm>
#include <regex>
#include <map>
#include <uwebsockets/App.h>

// ws = new WebSocket("ws://127.0.0.1:9001/");ws.onmessage = ({data}) => console.log("From server: ", data);

class PerSocketData {
public:
	std::string name;
	std::uint32_t user_id;
};

const std::string MESSAGE_TO = "message_to::";
const std::string SET_NAME = "set_name::";

bool isValidId(std::uint32_t last_user_id, std::uint32_t user_id) {
	if (user_id >= last_user_id) {
		return false;
	}
	else return true;
}

bool isSetName(std::string message) {
	return message.find(SET_NAME) == 0;
}

bool isValidName(std::string message) {
	if (message.length() >= 255) {
		return false;
	}
	else {
		std::size_t foundIndex = message.find("::");
		if (foundIndex != std::string::npos)
		{
			return false;
		}
		else return true;
	}
}

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

// сообщение пользователю с id
std::string messageTo(std::string user_id, std::string message) {
	return "message_to::" + user_id + "::" + message;
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

		std::cout << "New user connected, id = " << userData->user_id << std::endl;
		// вывод числа подключенных пользователей
		std::cout << "Total users connected: " << last_user_id - 1 << std::endl;

		// подписка пользователя на личный канал
		ws->subscribe("user#" + std::to_string(userData->user_id));
		// подписка пользователя на общий канал
		ws->subscribe("broadcast");
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
			if (isValidId(last_user_id, std::stoi(receiverId))) {
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
				std::cout << "User #" << transmitterId << " set his name" << std::endl;
			}
			else std::cout << "User #" << transmitterId << " INVALID NAME!" << std::endl;
		}

	 },
		// вызывается при отключении пользователя
		.close = [](auto* ws, int code, std::string_view /*message*/) {

		}
		 // прослушивание порта
		}).listen(9001, [](auto* listen_socket) {
			if (listen_socket) {
				std::cout << "Listening on port " << 9001 << std::endl;
			}
		}).run();
}
