import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class ShellTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory()
        cls.binary = pathlib.Path(cls.build.name) / 'termino'
        subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Wpedantic', '-pthread', str(ROOT / 'terminal.cpp'), '-o', str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def run_shell(self, commands):
        with tempfile.TemporaryDirectory() as directory:
            return subprocess.run([str(self.binary)], input=commands, text=True, capture_output=True, cwd=directory, timeout=5)

    def test_empty_input_and_eof(self):
        self.assertEqual(self.run_shell('   \n\n').returncode, 0)

    def test_missing_redirection_target(self):
        result = self.run_shell('echo hello >\necho survived\n')
        self.assertIn('requires a filename', result.stderr)
        self.assertIn('survived', result.stdout)

    def test_quoted_note(self):
        result = self.run_shell('notes add "Meeting at 5 PM"\nnotes view\n')
        self.assertIn('Meeting at 5 PM', result.stdout)

    def test_unquoted_multiword_note(self):
        self.assertIn('Meeting at 5 PM', self.run_shell('notes add Meeting at 5 PM\nnotes view\n').stdout)

    def test_invalid_reminder(self):
        result = self.run_shell('remindme abc hello\nremindme -1 hello\necho survived\n')
        self.assertEqual(result.stderr.count('Seconds must be'), 2)
        self.assertIn('survived', result.stdout)

    def test_pipeline(self):
        self.assertIn('HELLO', self.run_shell('echo hello | tr a-z A-Z\n').stdout)

    def test_invalid_pipeline(self):
        result = self.run_shell('| cat\necho x |\necho x | | cat\n&\n')
        self.assertEqual(result.returncode, 0)
        self.assertIn('Missing pipeline command', result.stderr)

    def test_unclosed_quote(self):
        self.assertIn('Unterminated', self.run_shell('echo "hello\n').stderr)

    def test_redirection(self):
        result = self.run_shell('echo first > out\necho second >> out\ncat < out\n')
        self.assertIn('first\nsecond', result.stdout)

    def test_weather_rejects_shell_metacharacters(self):
        result = self.run_shell('fetchweather x;echo\n')
        self.assertIn('Use letters', result.stderr)

if __name__ == '__main__':
    unittest.main()
