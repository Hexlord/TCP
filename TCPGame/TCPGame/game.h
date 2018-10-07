#pragma once

#include <string>

#define DEFAULT_PLAYER_COUNT 16

enum class GameState : int
{
	AWAIT_PLAYERS,
	AWAIT_START,
	AWAIT_BETS,
	AWAIT_RESTART,
};

enum class SlotState : int
{
	EMPTY,
	AWAIT_ROLE,
	PLAYER,
	ADMIN,
	EVENT_LISTENER,
};



class Game
{
public:
	Game(){}
	~Game() {};
	Game(const Game& other) = delete;

	void init();

	int connect(int slot);

	int role(int slot, SlotState state, char* sBack);


	int bets(int slot, char* sBack);

	int bet(int slot, int _type, int _bet, char* sBack);

	int roulette(int slot, char* sBack);

	int restart(int slot, char* sBack);

	int getbets(int slot, char* out);

	int getmoney(int slot, char* out);

	SlotState getSlot(int i)
	{
		return m_slots[i];
	}

	const char* getresult();

	const char* getrole(int slot);


private:

	void play(int slot, bool winCond, int coef);

	GameState m_state;
	int m_userCount;
	int m_roulette;
	int m_betTypes[DEFAULT_PLAYER_COUNT];
	int m_bets[DEFAULT_PLAYER_COUNT];
	int m_money[DEFAULT_PLAYER_COUNT];
	SlotState m_slots[DEFAULT_PLAYER_COUNT];

};