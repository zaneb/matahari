import core
import shellmode
import interpreter

class MatahariShell(object):
    def __init__(self, prompt='mhsh'):
        self.manager = core.Manager()
        initial_mode = shellmode.RootMode(self.manager)
        self.interpreter = interpreter.Interpreter(prompt, initial_mode)

    def __call__(self):
        self.interpreter.cmdloop()
