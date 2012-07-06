ALTER TABLE `character_stats` DROP COLUMN `resilience`;
DROP TABLE IF EXISTS `character_arena_stats`;
DROP TABLE IF EXISTS `arena_team`;
DROP TABLE IF EXISTS `arena_team_member`;
DROP TABLE IF EXISTS `character_battleground_random`;
DROP TABLE IF EXISTS `character_achievement`;
DROP TABLE IF EXISTS `character_achievement_progress`;
DROP TABLE IF EXISTS `character_glyphs`;
DROP TABLE IF EXISTS `guild_bank_eventlog`;
DROP TABLE IF EXISTS `guild_bank_item`;
DROP TABLE IF EXISTS `guild_bank_right`;
DROP TABLE IF EXISTS `guild_bank_tab`;

ALTER TABLE guild DROP COLUMN `BankMoney`;
ALTER TABLE guild_rank DROP COLUMN `BankMoneyPerDay`;
ALTER TABLE groups DROP COLUMN difficulty,raiddifficulty;
ALTER TABLE instance DROP COLUMN difficulty;
ALTER TABLE instance_reset DROP COLUMN difficulty;
ALTER TABLE character_instance DROP COLUMN difficulty;
ALTER TABLE characters DROP COLUMN dungeon_difficulty;

ALTER TABLE guild_member
    DROP COLUMN BankResetTimeMoney,
    DROP COLUMN BankRemMoney,
    DROP COLUMN BankResetTimeTab0,
    DROP COLUMN BankRemSlotsTab0,
    DROP COLUMN BankResetTimeTab1,
    DROP COLUMN BankRemSlotsTab1,
    DROP COLUMN BankResetTimeTab2,
    DROP COLUMN BankRemSlotsTab2,
    DROP COLUMN BankResetTimeTab3,
    DROP COLUMN BankRemSlotsTab3,
    DROP COLUMN BankResetTimeTab4,
    DROP COLUMN BankRemSlotsTab4,
    DROP COLUMN BankResetTimeTab5,
    DROP COLUMN BankRemSlotsTab5;
