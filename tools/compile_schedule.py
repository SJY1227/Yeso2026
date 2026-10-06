"""Compile a development JSON schedule to StateCodec v1. No server DTO assumed."""
import argparse
import json
import struct
import zlib
from datetime import datetime
from pathlib import Path

def integer(value, low, high):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f'Expected integer in {low}..{high}')
    return value

def text(value):
    if not isinstance(value,str) or not value or any(ord(c)<32 or ord(c)==127 for c in value):
        raise ValueError('Text must be nonempty and contain no control characters')
    encoded=value.encode('utf-8')
    if len(encoded)>95: raise ValueError('Text exceeds 95 UTF-8 bytes')
    return bytes([len(encoded)])+encoded

def timestamp(value):
    dt=datetime.fromisoformat(value.replace('Z','+00:00'))
    if dt.tzinfo is None: raise ValueError('Timestamps must include an explicit UTC offset')
    if dt.microsecond: raise ValueError('Use whole-second timestamps')
    return integer(int(dt.timestamp()),0,2**63-1)

def compile_schedule(source):
    revision=integer(source['revision'],1,2**64-1)
    runs=source['runs']
    if not isinstance(runs,list) or not 1<=len(runs)<=16: raise ValueError('Expected 1..16 runs')
    result=bytearray(struct.pack('<IHQQB',0x54534452,1,0,revision,len(runs)))
    ids=set(); windows=[]
    for run in runs:
        run_id=integer(run['id'],1,2**64-1)
        if run_id in ids: raise ValueError('Duplicate run id')
        ids.add(run_id)
        start,end=timestamp(run['startsAt']),timestamp(run['endsAt'])
        if end<=start: raise ValueError('Invalid run window')
        rule={'deadline-wins':0,'allow-at-deadline':1}[run['deadlineRule']]
        for a,b,other_rule in windows:
            if start<b and a<end or start==b and other_rule or a==end and rule:
                raise ValueError('Overlapping run windows')
        windows.append((start,end,rule))
        warning=integer(run['warningLeadSeconds'],0,2**32-1)
        steps=run['steps']
        if not isinstance(steps,list) or not 1<=len(steps)<=16: raise ValueError('Expected 1..16 steps')
        result+=struct.pack('<QqqBBIBIBHBqHB',run_id,start,end,len(steps),rule,warning,0,0,0,0,0,0,0,0)
        result+=text(run['title']); step_ids=set()
        for step in steps:
            step_id=integer(step['id'],1,2**32-1)
            if step_id in step_ids: raise ValueError('Duplicate step id')
            step_ids.add(step_id)
            result+=struct.pack('<I',step_id)+text(step['title'])
    result+=struct.pack('<H',0)
    result+=struct.pack('<I',zlib.crc32(result))
    if len(result)>65536: raise ValueError('Payload too large')
    return bytes(result)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('input',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args()
    try: payload=compile_schedule(json.loads(args.input.read_text(encoding='utf-8-sig')))
    except (ValueError,TypeError,KeyError,OverflowError) as error: parser.error(str(error))
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(payload)
    print(f'Compiled {len(payload)} bytes to {args.output}')
