var webpack = require('webpack')
const HtmlWebpackPlugin = require('html-webpack-plugin')
const path = require('path')

module.exports = {

  mode: 'development',
  entry: {
    main: './src/index.js',
  },

  output: {
    path: path.resolve(__dirname + '/dist'),
    filename: '[name].bundle.js',
  },

  devServer: {
    static: './dist',
    client: {
      overlay: false
    }
  },

  resolve: {
    alias: {
      'three$': path.resolve(__dirname, './node_modules/three/build/three.module.js')
    }
  },

  optimization: {
    minimize: false,
    usedExports: true,
    splitChunks: {
      chunks: 'all',
      cacheGroups: {
        vendor: {
          test: /[\\/]node_modules[\\/]/,
          name: 'vendors',
          priority: 10
        }
      }
    }
  },
  performance: {
    hints: false
  },
  plugins: [
    new webpack.HotModuleReplacementPlugin(),
    new HtmlWebpackPlugin({
      title: 'Nexus3D',
      template: __dirname + '/dist/index.html',
      inject: 'body'
    }),
  ],
}
