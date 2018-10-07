#include "game.h"
#include "network.h"

const char* g_bet_type[] =
{ "Zero","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20",
"21","22","23","24","25","26","27","28","29","30","31","32","33","34","35","36","Black","White","Even","Odd"};

const char* g_role[] =
{
	"-",
	"Unassigned",
	"Player",
	"Admin",
	"Event listener"
};

bool isZero(int n)
{
	return n == 0;
}

bool isBlack(int n)
{
	int black[] = { 2,4,6,8,10,11,13,15,17,20,22,24,26,28,29,31,33,35 };
	for (int i = 0; i < sizeof(black) / sizeof(int); ++i)
	{
		if (black[i] == n) return true;
	}
	return false;
}

bool isRed(int n)
{
	return (!isZero(n) && !isBlack(n));
}

bool isEven(int n)
{
	return n % 2 == 0;
}

bool isOdd(int n)
{
	return !isEven(n);
}
void Game::init()
{
	m_state = GameState::AWAIT_PLAYERS;
	ZeroMemory(m_bets, sizeof(m_bets));
	ZeroMemory(m_betTypes, sizeof(m_betTypes));
	for (int i = 0; i < DEFAULT_PLAYER_COUNT; ++i)
	{
		m_money[i] = 1000;
	}
	memset(m_slots, (char)SlotState::EMPTY, sizeof(m_slots));
	m_userCount = 0;

	printf("Game initialized, awaiting players\n");
}

int Game::connect(int slot)
{
	if (m_state != GameState::AWAIT_PLAYERS)
	{
		printf("Game is not in await players state, connection blocked on slot %d\n", slot);
		return 1;
	}
	m_slots[slot] = SlotState::AWAIT_ROLE;

	return 0;
}

int Game::role(int slot, SlotState state, char* sBack)
{
	if (m_state != GameState::AWAIT_PLAYERS)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Game is not in await players state, role change blocked");
		return 1;
	}
	if (m_slots[slot] != SlotState::AWAIT_ROLE)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Role already chosen, role change blocked");
		return 1;
	}
	if (!(state == SlotState::PLAYER || state == SlotState::ADMIN || state == SlotState::EVENT_LISTENER))
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Unknown role. A for admin, P for player");
		return 1;
	}
	int adminCount = 0;
	for (int i = 0; i < DEFAULT_PLAYER_COUNT; ++i)
	{
		if (m_slots[i] == SlotState::ADMIN)
		{
			++adminCount;
		}
	}
	if (state == SlotState::ADMIN && adminCount > 0)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Admin role already occupied");
		return 1;
	}
	m_slots[slot] = state;
	strcpy_s(sBack, DEFAULT_PORTION, "OKK:");
	return 0;
}

int Game::bets(int slot, char* sBack)
{
	if (m_state != GameState::AWAIT_PLAYERS)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Game is not in await players state, ignoring bets command");
		return 1;
	}
	if (m_slots[slot] != SlotState::ADMIN)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Insufficient permissions for bets command");
		return 1;
	}
	int playerCount = 0;
	for (int i = 0; i < DEFAULT_PLAYER_COUNT; ++i)
	{
		if (m_slots[i] == SlotState::PLAYER)
		{
			++playerCount;
		}
	}
	if (playerCount == 0)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:At least 1 player is required for game to start, ignoring bets command");
		return 1;
	}
	ZeroMemory(m_bets, sizeof(m_bets));
	ZeroMemory(m_betTypes, sizeof(m_betTypes));
	m_state = GameState::AWAIT_BETS;
	strcpy_s(sBack, DEFAULT_PORTION, "OKK:");
	return 0;
}

int Game::bet(int slot, int _type, int _bet, char* sBack)
{
	if (m_state != GameState::AWAIT_BETS)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Game is not in bets state, ignoring bet");
		return 1;
	}
	if (m_slots[slot] != SlotState::PLAYER)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Only players can bet, ignoring bet");
		return 1;
	}
	if (_bet <= 0)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Bet amount must be greater than zero");
		return 1;
	}
	auto& money = m_money[slot];
	auto& bet = m_bets[slot];
	if (bet > 0)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:User already has a bet, ignoring");
		return 1;
	}
	if (money < _bet)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Insufficient money");
		return 1;
	}
	money -= _bet;
	bet = _bet;
	m_betTypes[slot] = _type;
	strcpy_s(sBack, DEFAULT_PORTION, "OKK:");
	return 0;
}

int Game::roulette(int slot, char* sBack)
{
	if (m_state != GameState::AWAIT_BETS)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Game is not in await roulette state, ignoring roulette command");
		return 1;
	}
	if (m_slots[slot] != SlotState::ADMIN)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Insufficient permissions for roulette command");
		return 1;
	}
	int sum = 0;
	for (int i = 0; i < DEFAULT_PLAYER_COUNT; ++i)
	{
		sum += m_bets[i];
	}
	if (sum == 0)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:No bets placed");
		return 1;
	}

	m_roulette = rand() % 37; // 0..36

	for (int i = 0; i < DEFAULT_PLAYER_COUNT; ++i)
	{
		int bet = m_bets[i];
		if (bet == 0) continue;
		int type = m_betTypes[i];
		if (type == 0) play(i, isZero(m_roulette), 35);
		else if (type == 37) play(i, isBlack(m_roulette), 1);
		else if (type == 38) play(i, isRed(m_roulette), 1);
		else if (type == 39) play(i, isEven(m_roulette), 1);
		else if (type == 40) play(i, isOdd(m_roulette), 1);
		else play(i, m_roulette == type, 35);
	}

	ZeroMemory(m_bets, sizeof(m_bets));
	m_state = GameState::AWAIT_RESTART;
	strcpy_s(sBack, DEFAULT_PORTION, "OKK:");
	return 0;
}

int Game::restart(int slot, char* sBack)
{
	if (m_state != GameState::AWAIT_RESTART)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Game is not in await restart state, ignoring restart command");
		return 1;
	}
	if (m_slots[slot] != SlotState::ADMIN)
	{
		strcpy_s(sBack, DEFAULT_PORTION, "ERR:Insufficient permissions for restart command");
		return 1;
	}
	m_state = GameState::AWAIT_BETS;
	strcpy_s(sBack, DEFAULT_PORTION, "OKK:");
	return 0;
}

int Game::getbets(int slot, char* out)
{
	if (m_state != GameState::AWAIT_BETS)
	{
		strcpy_s(out, DEFAULT_PORTION, "ERR:Game is not in bets state, ignoring bets info command");
		return 1;
	}

	std::string s = "OKK:Bets:\n";
	for (int i = 0; i < DEFAULT_PLAYER_COUNT; ++i)
	{
		if (m_slots[i] == SlotState::PLAYER)
		{
			s += "User ";
			s += std::to_string(i);
			s += ": ";
			s += std::to_string(m_bets[i]);
			s += " on ";
			s += g_bet_type[m_betTypes[i]];
			s += "\n";
		}
	}
	strcpy_s(out, DEFAULT_PORTION, s.c_str());
	return 0;
}

int Game::getmoney(int slot, char* out)
{
	std::string s = "OKK:Money:\n";
	for (int i = 0; i < DEFAULT_PLAYER_COUNT; ++i)
	{
		if (m_slots[i] == SlotState::PLAYER)
		{
			s += "User ";
			s += std::to_string(i);
			s += ": ";
			s += std::to_string(m_money[i]);
			s += "\n";
		}
	}
	strcpy_s(out, DEFAULT_PORTION, s.c_str());
	return 0;
}

const char* Game::getresult()
{
	return g_bet_type[m_roulette];
}

const char* Game::getrole(int slot)
{
	return g_role[(int)m_slots[slot]];
}

void Game::play(int slot, bool winCond, int coef)
{
	if (winCond)
	{
		m_money[slot] += m_bets[slot] * coef;
	}
}
