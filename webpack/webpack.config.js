var webpack = require('webpack')

module.exports = {

  entry: {
    main: './src/index.js',
  },

  output: {
    path: __dirname + '/dist',
    filename: 'main.js',
  },

  optimization: {
    minimize: false
  }    
}
