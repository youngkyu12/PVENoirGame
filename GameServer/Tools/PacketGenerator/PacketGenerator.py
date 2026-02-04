import argparse
import jinja2

def main():

    arg_parser = argparse.ArgumentParser(description="PacketGenerator")
    arg_parser.add_argument('--path', type=str, default='', help='proto path')
    arg_parser.add_argument('--output', type=str, default='TestPacketHandler', help='output file')
    arg_parser.add_argument('--recv', type=str, default='C_', help='recv convention')
    arg_parser.add_argument('--send', type=str, default='S_', help='send convention')




    return

if __name__ == "__main__":
    main()
