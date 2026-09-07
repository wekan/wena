#!/usr/bin/env python3
import re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];WEKAN=ROOT.parents[1]
SOURCE=WEKAN/'imports/lib/legacyHtml4.js';CONTRACT=ROOT/'imports/ui/page_contract.c'
PIN='689a393841f08c3a020a4ef435b869b7641b21df'
assert subprocess.check_output(['git','-C',str(WEKAN),'rev-parse','HEAD'],text=True).strip()==PIN
js=SOURCE.read_text(encoding='utf-8');c=CONTRACT.read_text(encoding='utf-8')
def block(name):
 body=re.search(rf'const {name} = \{{(.*?)\n\}};',js,re.S).group(1)
 return dict(re.findall(r"([a-z][a-z0-9]*):\s*'(#[0-9a-fA-F]{6})'",body))
expected=block('BOARD_THEME_COLORS');expected.update(block('ITEM_COLORS'))
actual=dict(re.findall(r'\{"([a-z][a-z0-9]*)", "(#[0-9a-fA-F]{6})"\}',c))
assert actual==expected,(sorted(set(expected)-set(actual)),sorted(set(actual)-set(expected)))
assert len(expected)==50
