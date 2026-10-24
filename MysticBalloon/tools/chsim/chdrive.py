#!/usr/bin/env python3
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "CHGame/tools/chsim"))
from chdrivelib import main
if __name__ == "__main__":
    main(ident="MYBL ")
