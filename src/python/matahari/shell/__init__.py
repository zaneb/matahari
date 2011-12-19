import matahari.api
import mode
import interpreter

class MatahariShell(object):
    def __init__(self, prompt='mhsh'):
        self.state = matahari.api.Matahari()
        initial_mode = mode.RootMode(self.state)
        self.interpreter = interpreter.Interpreter(prompt, initial_mode, debug=True)

    def __call__(self):
        self.interpreter.cmdloop()
