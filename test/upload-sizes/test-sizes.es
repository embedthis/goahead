let http: Http = new Http

let i
for (i = 0; i < 2048; i++) {
    let buf = 'a'.times(i) + '\n'
    Path('a.dat').write(buf)

    http.upload('http://127.0.0.1:18080/action/uploadTest', { myfile: 'a.dat' } )
    http.wait()

    if (http.status != 200) {
        print('Failed', i)
        break
    } else {
        let data = Path('../web/tmp/a.dat').readString()
        if (data.toString() == buf.toString()) {
            print('Passed', i)
        } else {
            print('Failed')
            print('DATA', data.length) ; print(data)
            print('BUF', buf.length) ; print(buf)
            break
        }
    }
    http.reset()
}
