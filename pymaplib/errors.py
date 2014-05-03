
class MapLibError(RuntimeError):
    pass

class FileLoadError(MapLibError):
    pass

class FileOpenError(FileLoadError):
    pass

class FileParseError(FileLoadError):
    pass

