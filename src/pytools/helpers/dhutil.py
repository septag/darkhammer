import os, fnmatch, glob

class Util():
    @staticmethod
    def glob_recursive(treeroot, patterns):
        results = []
        for base, dirs, files in os.walk(treeroot):
            filenames = []
            for pattern in patterns:
                filenames.extend(fnmatch.filter(files, pattern))
            results.extend(os.path.join(base, f) for f in filenames)

        return results

