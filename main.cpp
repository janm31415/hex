#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <stdint.h>
#include <sstream>

struct undo_char_info
  {
  std::fstream::pos_type pos;
  char orig_char;
  char new_char;  
  };

struct undo_info
  {
  std::vector<undo_char_info> undo_chars;
  };

std::vector<undo_info> undo, redo;

int64_t addr = 0;

bool flip_bits = false;

bool is_little_endian()
  {
  short int number = 0x1;
  char *num_ptr = (char*)&number;
  return (num_ptr[0] == 1);
  }

void print_help(std::ostream& ostr)
  {
  ostr << "Available commands:" << std::endl;
  ostr << "  ENTER : dump current 256 addresses" << std::endl;
  ostr << "  u : scroll up (address - 256)" << std::endl;
  ostr << "  d : scroll down (address + 256)" << std::endl;
  ostr << "  n : next address" << std::endl;
  ostr << "  p : previous address" << std::endl;
  ostr << "  goto <addr> : go to an address in hexadecimal notation" << std::endl;
  ostr << "  find <string> : find a string literal" << std::endl;
  ostr << "  dump : dump the whole file" << std::endl;
  ostr << "  puthex <hex> : modifies the current address to <hex>" << std::endl;
  ostr << "  put, putchar <char> : modifies the current address to <char>" << std::endl;
  ostr << "  write <string> : modifies the current and subsequent addresses to <string>" << std::endl;
  ostr << "  undo : undoes the last write or put" << std::endl;
  ostr << "  undoall : undoes all the changes" << std::endl;
  ostr << "  redo : redoes the last change that was undone" << std::endl;
  ostr << "  redoall : redoes all the undone changes" << std::endl;
  ostr << "  endianness : shows this PCs endianness" << std::endl;
  ostr << "  little : consider the input file as little-endian" << std::endl;
  ostr << "  big : consider the input file as big-endian" << std::endl;
  ostr << "  >> <file> : stream output to a file" << std::endl;
  ostr << "  q, quit, exit : exit the application" << std::endl;
  }

std::string int_to_hex(uint8_t i)
  {
  std::string hex;
  int h1 = (i >> 4) & 0x0f;
  if (h1 < 10)
    hex += '0' + h1;
  else
    hex += 'A' + h1 - 10;
  int h2 = (i) & 0x0f;
  if (h2 < 10)
    hex += '0' + h2;
  else
    hex += 'A' + h2 - 10;
  return hex;
  }

std::string int_to_hex(uint16_t i)
  {
  std::string hex;
  uint8_t h1 = (i >> 8) & 0x00ff;
  uint8_t h2 = i & 0x00ff;  
  return int_to_hex(h1) + int_to_hex(h2);
  }

std::string int_to_hex(uint32_t i)
  {
  std::string hex;
  uint16_t h1 = (i >> 16) & 0x0000ffff;
  uint16_t h2 = i & 0x0000ffff;
  return int_to_hex(h1) + int_to_hex(h2);
  }

std::string int_to_hex(char ch)
  {
  uint8_t* c = reinterpret_cast<uint8_t*>(&ch);
  return int_to_hex(*c);
  }

uint32_t hex_to_int(const std::string& s)
  {
  std::stringstream sstr;
  sstr << std::hex << s;
  uint32_t x;
  sstr >> x;
  return x;
  }

char to_str(char c)
  {  
  if (c >= 32 && c < 127)
    {
    return c;
    }
  else
    {
    if (c < 0)
      {
      return c;
      }
    else
      return '.';
    }      
  }

void undo_eof_state(std::fstream& _file)
  {
  if (!_file)
    {
    _file.clear();
    _file.seekg(addr);    
    }
  }

char flip_bits_of_char(char b)
  {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
  }

void dump(std::fstream& _file, std::ostream& ostr)
  {
  auto address = _file.tellg();
  char buffer[256];
  _file.read(buffer, 256);
  std::streamsize chars_read = 256;
  if (!_file)
    {
    chars_read = _file.gcount();    
    }
  if (!chars_read)
    return;
  if (flip_bits)
    {
    for (auto i = 0; i < chars_read; ++i)
      buffer[i] = flip_bits_of_char(buffer[i]);
    }
  std::vector<char> characters;
  ostr << int_to_hex(uint32_t(address)) << ": ";
  for (auto i = 0; i < chars_read;++i)
    {
    ostr << int_to_hex(buffer[i]) << " ";
    characters.push_back(buffer[i]);
    if ((i + 1) % 16 == 0)
      {
      ostr << "| ";
      for (auto c : characters)
        {
        ostr << to_str(c);
        }
      characters.clear();
      ostr << std::endl;      
      if (i != chars_read - 1)
        {
        address += 16;
        ostr << int_to_hex(uint32_t(address)) << ": ";
        }
      }
    }
  if (chars_read % 16)
    {
    for (int i = 0; i < (16 - (chars_read % 16)); ++i)
      ostr << "   ";
    ostr << "| ";
    for (auto c : characters)
      ostr << to_str(c);
    ostr << std::endl;
    }  
  }

void dump(std::fstream& _file, std::streampos pos, std::ostream& ostr)
  {
  _file.seekg(pos);
  dump(_file, ostr);
  }

void dump_full(std::fstream& _file, std::ostream& ostr)
  {
  _file.seekg(0);
  while (_file)
    dump(_file, ostr);
  }

int count_connectors(std::string temp)
  {
  auto it = temp.find_first_of('"');
  int found = 0;
  while (it != std::string::npos)
    {
    ++found;
    temp.erase(temp.begin(), temp.begin() + it + 1);
    it = temp.find_first_of('"');
    }
  return found;
  }

std::vector<std::string> get_arguments(const std::string& command)
  {
  std::vector<std::string> output;
  std::stringstream sstr;
  sstr << command;
  bool connector_present = false;
  while (!sstr.eof())
    {
    std::string temp;
    sstr >> temp;
    int connectors_found = count_connectors(temp);
    bool connector_found = false;
    if (connectors_found % 2 == 1)
      connector_found = true;
    if (connector_present)
      {
      output.back() += " ";
      output.back() += temp;
      if (connector_found)
        connector_present = false;
      }
    else
      {
      output.push_back(temp);
      if (connector_found)
        connector_present = true;
      }
    }
  return output;
  }

std::fstream::pos_type _find(std::fstream& _file, const std::string& target)
  {
  _file.seekg(int(addr)+1);
  std::string line;
  while (std::getline(_file, line))
    {
    auto pos = line.find(target, 0);
    if (pos != std::string::npos)
      {
      return int(_file.tellg()) - int(line.length()) + int(pos) - 1;
      }
    }
  return std::fstream::pos_type(addr);
  }

void _put(undo_info& ui, std::fstream& _file, std::fstream::pos_type addr, char ch)
  {
  undo_char_info uci;
  _file.seekg(addr);
  char orig = _file.get();
  _file.seekp(addr);
  if (!_file)
    {    
    _file.clear();
    return;
    }  
  if (flip_bits)
    ch = flip_bits_of_char(ch);
  _file.put(ch);    
  uci.orig_char = orig;
  uci.new_char = ch;
  uci.pos = addr;
  ui.undo_chars.push_back(uci);
  }

void _put(std::fstream& _file, std::fstream::pos_type addr, char ch)
  {  
  undo_info ui;
  _put(ui, _file, addr, ch);
  undo.push_back(ui);
  }

void _write(std::fstream& _file, std::fstream::pos_type addr, const std::string& str)
  {
  undo_info ui;
  for (auto ch : str)
    {
    _put(ui, _file, addr, ch);
    addr = uint64_t(addr) + 1;
    }
  undo.push_back(ui);
  }

std::fstream::pos_type _undo(std::fstream& _file)
  {
  if (!undo.empty())
    {
    auto ui = undo.back();
    for (auto uci : ui.undo_chars)
      {
      _file.seekp(uci.pos);
      _file.put(uci.orig_char);
      }    
    redo.push_back(ui);
    undo.pop_back();
    return ui.undo_chars.front().pos;
    }
  return _file.tellg();
  }

std::fstream::pos_type _redo(std::fstream& _file)
  {
  if (!redo.empty())
    {
    auto ui = redo.back();
    for (auto uci : ui.undo_chars)
      {
      _file.seekp(uci.pos);      
      _file.put(uci.new_char);
      }
    undo.push_back(ui);
    redo.pop_back();
    return ui.undo_chars.front().pos;
    }
  return _file.tellg();
  }

void parse(const std::string& command, std::fstream& _file)
  {  
  auto arguments = get_arguments(command);
  size_t argc = arguments.size();  

  std::ostream* _stream = &std::cout;
  std::string output_filename;
  std::ofstream file;
  bool use_console_out = true;
  bool _dump = false;
  bool full = false;
  bool find = false;
  bool help = false;
  bool show_endianness = false;
  std::string target;  

  for (int i = 0; i < argc; ++i)
    {
    if (argc == 1 && arguments[i] == "")
      {
      _dump = true;
      }
    else if (arguments[i] == "-?" || arguments[i] == "?" || arguments[i] == "help")
      {
      help = true;
      }
    else if (arguments[i] == ">>" && (i < (argc - 1)))
      {
      ++i;
      output_filename = arguments[i];
      use_console_out = false;
      if (argc == 2)
        _dump = true;
      }
    else if (arguments[i].find(">>") == 0)
      {      
      arguments[i].erase(arguments[i].begin(), arguments[i].begin() + 2);
      output_filename = arguments[i];
      use_console_out = false;
      if (argc == 1)
        _dump = true;
      }
    else if (arguments[i] == "goto" && (i < (argc - 1)))
      {
      ++i;
      addr = hex_to_int(arguments[i]);
      _dump = true;
      }
    else if (arguments[i] == "find" && (i < (argc - 1)))
      {
      ++i;
      target = arguments[i];
      find = true;      
      }
    else if (arguments[i] == "u")
      {
      addr -= 256;
      if (addr < 0)
        addr = 0;      
      _dump = true;
      }
    else if (arguments[i] == "d")
      {
      addr += 256;
      _dump = true;
      }
    else if (arguments[i] == "n")
      {
      addr += 1;
      _dump = true;
      }
    else if (arguments[i] == "p")
      {
      addr -= 1;      
      if (addr < 0)
        addr = 0;
      _dump = true;
      }
    else if (arguments[i] == "dump")
      {
      full = true;
      }
    else if (arguments[i] == "puthex" && (i < (argc - 1)))
      {
      ++i;
      auto ch = hex_to_int(arguments[i]);
      _put(_file, addr, char(ch));
      _file.seekg(addr);
      _dump = true;      
      }
    else if ((arguments[i] == "putchar" || arguments[i] == "put") && (i < (argc - 1)))
      {      
      ++i;
      auto ch = arguments[i][0];
      _put(_file, addr, char(ch));
      _file.seekg(addr);
      _dump = true;
      }
    else if (arguments[i] == "write" && (i < (argc - 1)))
      {
      i = int(argc);      
      auto p = command.find_first_of("write ");
      std::string target = command.substr(p + 6);
      _write(_file, addr, target);
      _file.seekg(addr);
      _dump = true;
      }
    else if (arguments[i] == "undo")
      {
      addr = _undo(_file);
      _dump = true;
      }
    else if (arguments[i] == "redo")
      {
      addr = _redo(_file);
      _dump = true;
      }
    else if (arguments[i] == "undoall")
      {
      while (!undo.empty())
        addr = _undo(_file);
      _dump = true;
      }
    else if (arguments[i] == "redoall")
      {
      while (!redo.empty())
        addr = _redo(_file);
      _dump = true;
      }
    else if (arguments[i] == "endianness")
      {
      show_endianness = true;
      }
    else if (arguments[i] == "little")
      {
      flip_bits = !is_little_endian();      
      _dump = true;
      }
    else if (arguments[i] == "big")
      {
      flip_bits = is_little_endian();
      _dump = true;
      }
    }

  if (!use_console_out)
    {
    file.open(output_filename);
    if (!file.is_open())
      use_console_out = true;
    else
      _stream = &file;
    }

  if (_dump)
    dump(_file, addr, *_stream);

  if (full)
    dump_full(_file, *_stream);

  if (find)
    {
    addr = _find(_file, target);    
    if (!_file)
      {
      *_stream << "Could not find " << target << std::endl << "Try again to search from the top." << std::endl;
      addr = 0;
      }
    else
      dump(_file, addr, *_stream);
    }

  if (show_endianness)
    {
    if (is_little_endian())
      *_stream << "I detected little-endian" << std::endl;
    else
      *_stream << "I detected big-endian" << std::endl;
    }

  if (help)
    print_help(*_stream);

  if (!use_console_out)
    file.close();

  undo_eof_state(_file);
  }

void hex_edit_file(std::fstream& _file)
  {
  dump(_file, 0, std::cout);
  undo_eof_state(_file);
  std::string command("");  
  while (command != "exit" && command != "quit" && command != "q")
    {
    std::cout << "> ";
    std::getline(std::cin, command);
  parse_command:
    parse(command, _file);
    }
  if (!undo.empty())
    {
    std::cout << "You've modified the file. Are you sure you want to keep the changes? [Y/N] ";
    char ch;
    std::cin >> ch;
    if (ch == 'n' || ch == 'N')
      {     
      std::cin.clear();
      std::cin.ignore(INT_MAX, '\n');
      command = "novalidcommand";
      std::cout << "Type undoall to undo all changes" << std::endl;
      goto parse_command;
      }
    }
  }

int main(int argc, char** argv)
  {
  if (argc < 2)
    {
    std::cout << "Hex editor" << std::endl;
    std::cout << "Usage: Hex <file>" << std::endl;
    }
  else
    {
    std::fstream _file(argv[1], std::fstream::in | std::fstream::out | std::fstream::binary);
    if (!_file.is_open())
      {
      std::cout << "Error opening file " << argv[1] << std::endl;
      exit(1);
      }
    if (!is_little_endian())
      flip_bits = true;
    hex_edit_file(_file);
    _file.close();
    }
  return 0;
  }