import sys

TERM_DEFAULT = '\033[0m'
TERM_GREY = '\033[90m'
TERM_RED = '\033[31m'
TERM_GREEN = '\033[32m'
TERM_YELLOW = '\033[33m'
TERM_BLUE = '\033[34m'
TERM_WHITE = '\033[37m'
TERM_BOLDYELLOW = '\033[1m\033[33m'
TERM_BOLDBLUE = '\033[1m\033[34m'
TERM_BOLDMAGENTA = '\033[1m\033[35m'
TERM_BOLDRED = '\033[1m\033[31m'
TERM_BOLDWHITE = '\033[1m\033[37m'
TERM_BOLDGREEN = '\033[1m\033[32m'

class LogCon():
    @staticmethod
    def msg(msg, color = TERM_DEFAULT):
        if sys.platform != 'win32':
            sys.stdout.write(color + msg + TERM_DEFAULT)
        else:
            sys.stdout.write(msg)
        sys.stdout.flush()

    @staticmethod
    def msgline(msg, color = TERM_DEFAULT):
        if sys.platform != 'win32':
            sys.stdout.write(color + msg + '\n' + TERM_DEFAULT)
        else:
            sys.stdout.write(msg + '\n')
        sys.stdout.flush()

    @staticmethod
    def fatal(msg):
        if sys.platform != 'win32':
            sys.stdout.write(TERM_RED + msg + '\n' + TERM_DEFAULT)
        else:
            sys.stdout.write(msg + '\n')
        sys.stdout.flush()

    @staticmethod
    def warn(msg):
        if sys.platform != 'win32':
            sys.stdout.write(TERM_YELLOW + msg + '\n' + TERM_DEFAULT)
        else:
            sys.stdout.write(msg + '\n')
        sys.stdout.flush()


class Log():
    @staticmethod
    def msg(msg, color = TERM_DEFAULT):
        LogCon.msg(msg, color)

    @staticmethod
    def msgline(msg, color = TERM_DEFAULT):
        LogCon.msgline(msg, color)

    @staticmethod
    def fatal(msg):
        LogCon.fatal(msg)

    @staticmethod
    def warn(msg):
        LogCon.warn(msg)