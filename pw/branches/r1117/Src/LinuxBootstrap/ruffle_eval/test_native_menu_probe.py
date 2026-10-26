'''Mock menu evidence and input ownership; the real display gate remains opt-in.'''

from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

import native_menu_probe as probe
from test_gameplay_gate import fixture


def evidence(expected_moves=0):
	'''Extend the ordinary-session fixture with a manual ten-player lobby and nine bots.'''
	stdout, client = fixture('default', expected_moves=expected_moves)
	# Nine AI commands are separate from the single startup keepalive.
	before, after = 1 + expected_moves, 10 + expected_moves
	stdout = stdout.replace(f'commands={before} ', f'commands={after} ').replace(f'replayCommands={before} ', f'replayCommands={after} ')
	stdout = stdout.replace(f'commandBlocks={before}/{before}', f'commandBlocks={after}/{after}').replace(f'commands={before}/{before}', f'commands={after}/{after}')
	client += '''bootstrapCreateGame=no
bootstrapLegacyLobbyOverlay=no
bootstrapLegacyHeroOverlay=no
bootstrapLegacySessionOverlay=no
finalBootstrapInputScript=0/0/0.00
finalGameReplayInputPath=inactive
finalProductionLobbyCreatedGame=Maps/Multiplayer/MOBA/_.ADMPDSCR.xdb/10
finalProductionLobbyCallbacks=create:1 createMulti:1 createSingle:0 createRejected:0 join:0 spectate:0 reconnect:0 lastAction:create-multiplayer
finalProductionHeroCallbacks=ready:1 lastAction:ready
finalProductionLobbyTransition=1/create-multiplayer hero:yes active:no
finalProductionHeroLoadingTransition=1/ready loading:yes
finalStartingLobbyPresentation=production-xdb-lua productionFrames:10 legacyFrames:0
finalHeroLobbyPresentation=production-xdb-lua productionFrames:10 legacyFrames:0
finalGameWorldPlayers=10
finalGameWorldPresentPlayers=1
finalGameWorldPlayersWithHeroes=10
finalLinuxAIControllers=auto:9/9 add:9/9 bots:1/1 active:9 steps:1000 commands:9/9 fallback:0 combat:9
'''
	return stdout, client


class NativeMenuTests(unittest.TestCase):
	'''Reject automatic startup, silent bots, direct mutations and unsafe window input.'''

	def test_manual_session(self):
		result = probe.validate_menu(*evidence())
		self.assertEqual((result['bots'], result['ai_commands'], result['replay_commands']), (9, 9, 10))

	def prime_evidence(self):
		'''Bind natural death income to the ordinary session's actual local identity.'''
		stdout, client = evidence(1)
		stdout += 'Final live minimap commands: surface=yes proof=no input=1 move=1 attack=0 signal=0 select=0 projection=yes screen=200,600 action=live-minimap-move\n'
		stdout += 'Native nonhero prime: step=900 victim=8010 killer=8002 hero=7489 player=2 user=1984 source=presence amount=11 gold=100->111\n'
		client += '''finalLiveHeroObjectId=7489
finalLiveHeroPlayerId=2
finalLiveHeroUserId=1984
finalGameWorldAwardKillersCalls=1
finalGameWorldAwardKillersApplied=1
'''
		return stdout, client

	def test_natural_prime_receipt(self):
		result = probe.validate_menu(*self.prime_evidence(), require_prime_awards=True)
		self.assertEqual((result['prime_awards'], result['human_prime_receipts']), (1, 1))

	def test_prime_lane_input(self):
		with patch.object(probe, 'tool', return_value='42') as tool:
			probe.drive_prime_lane('42', 100)
			self.assertEqual([call.args for call in tool.call_args_list],
				[('getwindowfocus',), ('mousemove', '--window', '42', 200, 600), ('click', '--window', '42', 3)])
		with patch.object(probe, 'tool', return_value='99') as tool:
			with self.assertRaises(probe.EvidenceError):
				probe.drive_prime_lane('42', 100)
			self.assertEqual(tool.call_count, 1)

	def test_prime_requires_one_real_lane_move(self):
		stdout, client = self.prime_evidence()
		for old, new in (('input=1 move=1', 'input=0 move=0'), ('attack=0', 'attack=1'),
			('screen=200,600', 'screen=0,0'), ('proof=no', 'proof=yes'), ('projection=yes', 'projection=no')):
			with self.subTest(old=old), self.assertRaises(probe.EvidenceError):
				probe.validate_menu(stdout.replace(old, new), client, require_prime_awards=True)

	def test_prime_requires_paid_human_identity(self):
		stdout, client = self.prime_evidence()
		for old, new in (('hero=7489', 'hero=7490'), ('player=2', 'player=3'),
			('user=1984', 'user=-1'), ('source=presence', 'source=salary'),
			('amount=11', 'amount=nan'), ('amount=11', 'amount=0'),
			('gold=100->111', 'gold=100->100'), ('gold=100->111', 'gold=100->99'),
			('step=900', 'step=1001'), ('victim=8010', 'victim=-1')):
			with self.subTest(old=old), self.assertRaises(probe.EvidenceError):
				probe.validate_menu(stdout.replace(old, new), client, require_prime_awards=True)
		for invalid in ('', stdout + stdout.splitlines()[-1] + '\n'):
			with self.assertRaises(probe.EvidenceError):
				probe.validate_menu(invalid, client, require_prime_awards=True)

	def test_prime_counters_and_missing_records(self):
		stdout, client = self.prime_evidence()
		for line in client.splitlines():
			if line.startswith(('finalLiveHero', 'finalGameWorldAwardKillers')):
				for invalid in (client.replace(line + '\n', ''), client + line + '\n'):
					with self.subTest(line=line), self.assertRaises(probe.EvidenceError):
						probe.validate_menu(stdout, invalid, require_prime_awards=True)
		for old, new in (('KillersApplied=1', 'KillersApplied=0'),
			('KillersApplied=1', 'KillersApplied=2'), ('KillersCalls=1', 'KillersCalls=0')):
			with self.subTest(old=old), self.assertRaises(probe.EvidenceError):
				probe.validate_menu(stdout, client.replace(old, new), require_prime_awards=True)

	def test_bot_receipts_do_not_substitute_for_human(self):
		stdout, client = self.prime_evidence()
		bot = stdout.splitlines()[-1].replace('hero=7489 player=2 user=1984', 'hero=7490 player=3 user=-3')
		result = probe.validate_menu(stdout + bot + '\n', client, require_prime_awards=True)
		self.assertEqual(result['human_prime_receipts'], 1)
		with self.assertRaises(probe.EvidenceError):
			probe.validate_menu(stdout.replace(stdout.splitlines()[-1], bot), client, require_prime_awards=True)

	def test_empty_map_rejection_then_retry(self):
		stdout, client = evidence()
		stdout += 'Native lobby create rejected: empty-map\n'
		client = client.replace('create:1', 'create:2').replace('createMulti:1', 'createMulti:2').replace('createRejected:0', 'createRejected:1')
		self.assertEqual(probe.validate_menu(stdout, client, True)['bots'], 9)
		for bad_stdout, bad_client in (
			(stdout.replace('empty-map', 'invalid-capacity'), client),
			(stdout + 'Native lobby create rejected: empty-map\n', client),
			(stdout, client.replace('createRejected:1', 'createRejected:0')),
			(stdout, client.replace('create:2', 'create:1')),
			(stdout, client.replace('1/create-multiplayer hero', '2/create-multiplayer hero')),
			(stdout, client.replace('_.ADMPDSCR.xdb/10', 'missing/10'))):
			with self.subTest(client=bad_client), self.assertRaises(probe.EvidenceError):
				probe.validate_menu(bad_stdout, bad_client, True)
		with self.assertRaises(probe.EvidenceError):
			probe.validate_menu(stdout, client)

	def test_failed_rejection_never_retries_and_reaps(self):
		with tempfile.TemporaryDirectory() as directory:
			process = Mock()
			process.poll.return_value = None
			process.wait.return_value = 0
			with patch.object(probe.subprocess, 'Popen', return_value=process), \
				patch.object(probe, 'wait_for_window'), patch.object(probe, 'pause'), \
				patch.object(probe, 'owned_window', return_value='42'), patch.object(probe, 'click') as click, \
				patch.object(probe, 'drive') as drive, \
				patch.object(probe, 'wait_for', side_effect=[None, probe.EvidenceError('not rejected')]):
				with self.assertRaisesRegex(probe.EvidenceError, 'not rejected'):
					probe.run(Path('/client'), Path('/bin'), Path(directory) / 'out.log', reject_empty_first=True)
				self.assertEqual(click.call_args.args[:3], ('42', 551, 618))
				drive.assert_not_called()
				process.terminate.assert_called_once()
				process.wait.assert_called_once()

	def test_missing_and_duplicate_records(self):
		stdout, client = evidence()
		for line in client.splitlines():
			if line.startswith(('bootstrapCreate', 'bootstrapLegacy', 'finalProduction', 'finalStarting',
				'finalHeroLobby', 'finalLinuxAI', 'finalGameWorldPlayers', 'finalGameWorldPresent', 'finalBootstrap', 'finalGameReplayInput')):
				for text in (client.replace(line + '\n', ''), client + line + '\n'):
					with self.subTest(line=line), self.assertRaises(probe.EvidenceError):
						probe.validate_menu(stdout, text)

	def test_bad_menu_or_ai_evidence(self):
		stdout, client = evidence()
		for before, after in (
			('bootstrapCreateGame=no', 'bootstrapCreateGame=yes'),
			('bootstrapLegacyLobbyOverlay=no', 'bootstrapLegacyLobbyOverlay=yes'),
			('finalBootstrapInputScript=0/0/0.00', 'finalBootstrapInputScript=2/0/0.00'),
			('finalGameReplayInputPath=inactive', 'finalGameReplayInputPath=replay.pwrp'),
			('_.ADMPDSCR.xdb/10', '_.ADMPDSCR.xdb/2'), ('create:1', 'create:0'),
			('createMulti:1', 'createMulti:2'), ('join:0', 'join:1'), ('ready:1', 'ready:0'),
			('1/ready loading:yes', '0/none loading:yes'), ('productionFrames:10', 'productionFrames:0'),
			('legacyFrames:0', 'legacyFrames:1'), ('finalGameWorldPlayersWithHeroes=10', 'finalGameWorldPlayersWithHeroes=9'),
			('auto:9/9', 'auto:0/0'), ('add:9/9', 'add:9/8'), ('bots:1/1', 'bots:1/0'),
			('active:9', 'active:0'), ('steps:1000', 'steps:0'), ('commands:9/9', 'commands:9/8'),
			('commands:9/9', 'commands:10/10'), ('commands:9/9', 'commands:nan/nan'),
			('fallback:0', 'fallback:9'), ('combat:9', 'combat:0'),
			('bootstrapDiagnosticActions=0', 'bootstrapDiagnosticActions=1')):
			with self.subTest(before=before), self.assertRaises(probe.EvidenceError):
				probe.validate_menu(stdout, client.replace(before, after))

	def test_launch_has_no_automation(self):
		args = probe.command(Path('/client'), Path('/frame.png'), 120)
		flags = [arg for arg in args if arg.startswith('--bootstrap-')]
		self.assertEqual(flags, ['--bootstrap-frame-capture', '--bootstrap-frame-capture-after'])
		for seconds in (0, 119, 601, True, float('nan')):
			with self.subTest(seconds=seconds), self.assertRaises(probe.EvidenceError):
				probe.command(Path('/client'), Path('/frame.png'), seconds)

	def test_clicks_are_window_addressed(self):
		with patch.object(probe, 'tool', return_value='42') as tool, patch.object(probe, 'pause'):
			probe.drive('42', 100)
			self.assertEqual([call.args for call in tool.call_args_list if call.args[0] == 'click'],
				[('click', '--window', '42', 1)] * 3)
			self.assertEqual([call.args for call in tool.call_args_list if call.args[0] == 'mousemove'],
				[('mousemove', '--window', '42', 425, 360), ('mousemove', '--window', '42', 551, 618),
					('mousemove', '--window', '42', 640, 477)])

	def test_focus_loss_sends_no_input(self):
		with patch.object(probe, 'tool', return_value='99') as tool:
			with self.assertRaises(probe.EvidenceError):
				probe.drive('42', 100)
			self.assertEqual(tool.call_count, 1)

	def test_failure_reaps_child(self):
		for timeout in (False, True):
			process = Mock()
			process.poll.return_value = None
			process.wait.side_effect = [subprocess.TimeoutExpired('client', 10), 0] if timeout else [0]
			with tempfile.TemporaryDirectory() as directory, \
				patch.object(probe.subprocess, 'Popen', return_value=process), \
				patch.object(probe, 'wait_for_window', side_effect=probe.EvidenceError('no window')):
				with self.assertRaisesRegex(probe.EvidenceError, 'no window'):
					probe.run(Path('/client'), Path('/bin'), Path(directory) / 'output.log')
				process.terminate.assert_called_once()
				self.assertEqual(process.kill.call_count, int(timeout))
				self.assertEqual(process.wait.call_count, 1 + int(timeout))

	def test_driver_uses_the_real_evidence_gate(self):
		with tempfile.TemporaryDirectory() as directory:
			base = Path(directory)
			stdout, client = evidence()
			(base / 'linux-client-shell.log').write_text(client, encoding='utf-8')
			process = Mock()
			process.poll.return_value = process.wait.return_value = 0
			def launch(*args, **kwargs):
				kwargs['stdout'].write(stdout + f'Logs: {base}\n')
				return process
			with patch.object(probe.subprocess, 'Popen', side_effect=launch), \
				patch.object(probe, 'wait_for_window'), patch.object(probe, 'wait_for'), \
				patch.object(probe, 'pause'), patch.object(probe, 'owned_window', return_value='42'), \
				patch.object(probe, 'drive'):
				result, _ = probe.run(Path('/client'), base, base / 'out.log')
				self.assertEqual(result['ai_commands'], 9)
				process.terminate.assert_not_called()


if __name__ == '__main__':
	unittest.main()
