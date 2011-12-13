import api
import shellmode
import interpreter

class MatahariShell(object):
    def __init__(self, prompt='mhsh'):
        self.state = api.Matahari()
        initial_mode = shellmode.RootMode(self.state)
        self.interpreter = interpreter.Interpreter(prompt, initial_mode, debug=True)

    def __call__(self):
        self.interpreter.cmdloop()
