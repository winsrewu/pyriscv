stat = {"count": 0, "bitops": 0, "arithmeticops": 0, "switch_chance": 0}
arithmetic_count = {}
last = None
set_last = False
switched = 0

def add_count():
    global last, set_last, switched
    if not set_last:
        last = None
    set_last = False

    stat["count"] += 1

def add_bitops():
    global last, set_last, switched
    if last == "arithmeticops":
        switched += 1
    last = "bitops"
    set_last = True
    stat["bitops"] += 1

def add_arithmeticops(op_type = ""):
    if op_type != "":
        if not op_type in arithmetic_count:
            arithmetic_count[op_type] = 0
        arithmetic_count[op_type] += 1

    global last, set_last, switched
    if last == "bitops":
        switched += 1
    last = "arithmeticops"
    set_last = True
    stat["arithmeticops"] += 1

def get_stat():
    stat["switch_chance"] = switched / stat["count"]
    return stat

def get_arithmetic_count():
    return arithmetic_count