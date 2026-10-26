#!/usr/bin/env python3
'''Compare actual native replay gameplay with its completed original HUD session.'''

import argparse
from pathlib import Path

from gameplay_gate import fields, integer, number, pair, record, require, validate


def validate_replay(live_stdout, live_log, replay_stdout, replay_log):
	'''Require exact duration, actor identity and paid action receipts, not decoded totals alone.'''
	live = validate(live_stdout, live_log, 'targeting')
	for text in (replay_stdout, replay_log):
		require(text.endswith('\n') and '\0' not in text, 'Truncated replay evidence')
	require(record(replay_stdout, 'Prime World Linux client shell finished.', '') == '', 'Replay did not finish')
	require(record(replay_log, 'finalReplayFileLiveWorldCreated', '=') == 'no', 'Replay evidence includes a second diagnostic world')
	state = fields(record(replay_stdout, 'Final live replay input', ': '), '=')
	for key in ('active', 'ready', 'header', 'world', 'valid', 'stepMatch', 'commandMatch', 'statusMatch'):
		require(state.get(key) == 'yes', f'Replay {key} failed')
	require(state.get('error') == 'none', 'Replay reported an error')
	ticks = live['world_step'] + 1
	require(integer(state.get('segments', '')) == integer(state.get('calls', '')) == ticks and
		integer(state.get('finalStep', '')) == live['world_step'], 'Replay duration differs from live')
	require(pair(state.get('commands', '')) == (live['replay_commands'],) * 2, 'Replay command totals differ')
	capture = fields(record(live_stdout, 'Final replay capture validation', ': '), '=')
	require(pair(state.get('statuses', '')) == pair(capture.get('statusBlocks', '')), 'Replay status totals differ')
	ai = fields(record(replay_log, 'finalLinuxAIControllers', '='), ':')
	for key in ('auto', 'add', 'commands'):
		require(pair(ai.get(key, '')) == (0, 0), f'Replay generated autonomous {key}')
	for key in ('active', 'fallback'):
		require(integer(ai.get(key, '')) == 0, f'Replay has autonomous {key}')
	for suffix in ('ObjectId', 'PlayerId', 'UserId'):
		key = 'finalLiveHero' + suffix
		require(integer(record(live_log, key, '=')) == integer(record(replay_log, key, '=')),
			f'Replay hero {suffix} differs from live')
	for log in (live_log, replay_log):
		camera = fields(record(log, 'finalLocalMapCamera', '='), ':', 'active follow updates focusHeight')
		require(integer(camera['active']) == integer(camera['follow']) == 1 and
			integer(camera['updates']) > 0, 'Replay comparison requires recorded-human camera follow')
		number(camera['focusHeight'])
	for suffix in ('BaseYaw', 'RenderedYaw', 'Pitch', 'RenderedPitch', 'Zoom'):
		key = 'finalMap3DPreview' + suffix
		require(abs(number(record(live_log, key, '=')) - number(record(replay_log, key, '='))) <= .0001,
			f'Replay camera {suffix} differs from live')
	live_pan = pair(record(live_log, 'finalMap3DPreviewPan', '='), ',', number)
	replay_pan = pair(record(replay_log, 'finalMap3DPreviewPan', '='), ',', number)
	require(all(abs(a - b) <= .0001 for a, b in zip(live_pan, replay_pan)), 'Replay camera focus differs from live')
	for key in ('Native local talent purchase', 'Native local talent cast', 'Native talent formula payment'):
		require(record(live_stdout, key, ': ') == record(replay_stdout, key, ': '), f'Replay receipt differs: {key}')
	return dict(live, replay_ticks=ticks)


def evidence(path):
	'''Resolve the client's own log location from one completed captured stdout file.'''
	stdout = path.read_text(encoding='utf-8')
	log = Path(record(stdout, 'Logs', ': ')) / 'linux-client-shell.log'
	return stdout, log.read_text(encoding='utf-8')


def main():
	'''Validate existing captures without launching a client or altering recordings.'''
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument('--live-output', type=Path, required=True)
	parser.add_argument('--replay-output', type=Path, required=True)
	args = parser.parse_args()
	try:
		result = validate_replay(*evidence(args.live_output), *evidence(args.replay_output))
	except (OSError, ValueError) as error:
		parser.exit(1, f'Replay gameplay FAILED: {error}\n')
	print(f'Replay gameplay PASSED: {result}')


if __name__ == '__main__':
	main()
